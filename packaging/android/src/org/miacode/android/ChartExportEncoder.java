package org.miacode.android;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.media.MediaExtractor;
import android.media.MediaMetadataRetriever;
import android.graphics.Bitmap;
import android.opengl.*;
import android.view.Surface;
import org.json.JSONObject;
import java.io.File;
import java.io.RandomAccessFile;
import java.io.FileOutputStream;
import android.util.AtomicFile;
import java.nio.charset.StandardCharsets;
import java.nio.*;
import java.util.ArrayList;
import java.util.concurrent.*;

// Bounded input queue: the Qt renderer retains at most one pending chart frame.
// All codec, muxer and PV decode work runs on dedicated local worker threads.
public final class ChartExportEncoder {
    private final ArrayBlockingQueue<byte[]> frames = new ArrayBlockingQueue<>(2);
    private final ExecutorService videoDecoder = Executors.newSingleThreadExecutor();
    private volatile boolean cancelled, inputFinished, done, videoRequested;
    private volatile String error = "";
    private volatile int encodedFrames;
    private volatile byte[] videoPixels;
    private volatile int videoWidth, videoHeight;
    private MediaMetadataRetriever retriever;
    private final int width, height, fps, expectedFrames, audioBitrate, requestedVideoBitrate, gopSeconds;
    private int configuredVideoBitrate, bitrateMode;
    private String videoCodecName = "";
    private final String output, wav, videoPath;

    public ChartExportEncoder(String output, String wav, String videoPath, int width, int height,
                              int fps, int expectedFrames, int audioBitrate, int videoBitrate, int gopSeconds) {
        this.output = output; this.wav = wav; this.videoPath = videoPath;
        this.width = width; this.height = height; this.fps = fps;
        this.expectedFrames = expectedFrames; this.audioBitrate = audioBitrate;
        this.requestedVideoBitrate = videoBitrate; this.gopSeconds = gopSeconds;
        new Thread(this::run, "MiaCode chart encoder").start();
    }
    public boolean hasCapacity() { return !done && !cancelled && frames.remainingCapacity() > 0; }
    public boolean enqueue(byte[] rgba) {
        if (done || cancelled || rgba.length != width * height * 4) return false;
        return frames.offer(rgba);
    }
    public void finishInput() { inputFinished = true; }
    public void cancel() { cancelled = true; }
    public String status() {
        try { return new JSONObject().put("done", done).put("error", error)
            .put("cancelled", cancelled).put("frames", encodedFrames).toString(); }
        catch (Exception impossible) { return "{}"; }
    }
    public void requestVideoFrame(long secondUs) {
        if (videoPath.isEmpty() || videoRequested || cancelled || done) return;
        videoRequested = true; videoPixels = null;
        videoDecoder.execute(() -> {
            try {
                if (retriever == null) {
                    retriever = new MediaMetadataRetriever(); retriever.setDataSource(videoPath);
                    int w = Integer.parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH));
                    int h = Integer.parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT));
                    String rotation = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION);
                    if (rotation != null && (Integer.parseInt(rotation) % 180) != 0) { int t = w; w = h; h = t; }
                    double scale = Math.min(width / (double)w, height / (double)h);
                    videoWidth = Math.max(1, (int)Math.round(w * scale));
                    videoHeight = Math.max(1, (int)Math.round(h * scale));
                }
                Bitmap bitmap = retriever.getScaledFrameAtTime(Math.max(0, secondUs),
                    MediaMetadataRetriever.OPTION_CLOSEST, videoWidth, videoHeight);
                if (bitmap == null) throw new java.io.IOException("PV decoder returned no frame");
                try {
                    int w = bitmap.getWidth(), h = bitmap.getHeight();
                    int[] pixels = new int[w * h]; bitmap.getPixels(pixels, 0, w, 0, 0, w, h);
                    byte[] rgba = new byte[w * h * 4];
                    for (int i = 0; i < pixels.length; ++i) {
                        int argb = pixels[i]; int offset = i * 4;
                        rgba[offset] = (byte)(argb >> 16); rgba[offset + 1] = (byte)(argb >> 8);
                        rgba[offset + 2] = (byte)argb; rgba[offset + 3] = (byte)(argb >>> 24);
                    }
                    videoWidth = w; videoHeight = h; videoPixels = rgba;
                } finally { bitmap.recycle(); }
            } catch (Throwable failure) { error = failure.toString(); cancelled = true; }
        });
    }
    public byte[] takeVideoFrame() {
        byte[] pixels = videoPixels;
        if (pixels != null) { videoPixels = null; videoRequested = false; }
        return pixels;
    }
    public int videoWidth() { return videoWidth; }
    public int videoHeight() { return videoHeight; }

    private void run() {
        try { encode(); verifyContainer(); }
        catch (Throwable failure) { if (error.isEmpty()) error = failure.toString(); }
        finally {
            videoDecoder.shutdown();
            try { videoDecoder.awaitTermination(120, TimeUnit.SECONDS); }
            catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            if (retriever != null) { try { retriever.release(); } catch (Exception ignored) {} }
            frames.clear();
            done = true;
        }
    }
    private void verifyContainer() throws Exception {
        MediaExtractor extractor = new MediaExtractor();
        int videoCount = 0, audioCount = 0;
        long lastVideoPts = -1;
        try {
            extractor.setDataSource(output);
            for (int track = 0; track < extractor.getTrackCount(); ++track) {
                MediaFormat format = extractor.getTrackFormat(track);
                String mime = format.getString(MediaFormat.KEY_MIME);
                boolean isVideo = mime != null && mime.startsWith("video/");
                boolean isAudio = mime != null && mime.startsWith("audio/");
                if (!isVideo && !isAudio) continue;
                if (isVideo && (format.getInteger(MediaFormat.KEY_WIDTH) != width
                        || format.getInteger(MediaFormat.KEY_HEIGHT) != height))
                    throw new java.io.IOException("Encoded video dimensions mismatch");
                extractor.selectTrack(track);
                long previous = -1;
                while (extractor.getSampleTrackIndex() >= 0) {
                    long pts = extractor.getSampleTime();
                    if (pts < previous) throw new java.io.IOException("Encoded timestamps are not monotonic");
                    previous = pts;
                    if (isVideo) ++videoCount; else ++audioCount;
                    if (!extractor.advance()) break;
                }
                if (isVideo) lastVideoPts = previous;
                extractor.unselectTrack(track);
            }
        } finally { extractor.release(); }
        if (videoCount != expectedFrames || audioCount == 0
                || Math.abs(lastVideoPts - (expectedFrames - 1) * 1000000L / fps) > 2)
            throw new java.io.IOException("Encoded MP4 frame count, audio or timing mismatch");
        JSONObject report = new JSONObject().put("schema",1).put("width",width).put("height",height)
            .put("fps",fps).put("expectedFrames",expectedFrames).put("videoFrames",videoCount)
            .put("audioPackets",audioCount).put("lastVideoPtsUs",lastVideoPts)
            .put("pvEnabled",!videoPath.isEmpty()).put("containerVerified",true)
            .put("requestedVideoBitrateBps",requestedVideoBitrate).put("configuredVideoBitrateBps",configuredVideoBitrate)
            .put("audioBitrateKbps",audioBitrate).put("gopSeconds",gopSeconds)
            .put("bitrateMode",bitrateMode).put("videoCodec",videoCodecName);
        AtomicFile atomic = new AtomicFile(new File(output + ".json"));
        FileOutputStream file = atomic.startWrite();
        try { file.write(report.toString().getBytes(StandardCharsets.UTF_8)); atomic.finishWrite(file); }
        catch (Exception failure) { atomic.failWrite(file); throw failure; }
    }
    private void encode() throws Exception {
        MediaCodec video = null, audio = null;
        MediaMuxer muxer = null;
        Surface surface = null;
        EglTarget egl = null;
        boolean started = false;
        try (RandomAccessFile pcm = new RandomAccessFile(wav, "r")) {
            pcm.seek(44);
            MediaFormat vf = MediaFormat.createVideoFormat("video/avc", width, height);
            vf.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
            video = MediaCodec.createEncoderByType("video/avc");
            videoCodecName = video.getName();
            MediaCodecInfo.CodecCapabilities capabilities = video.getCodecInfo().getCapabilitiesForType("video/avc");
            configuredVideoBitrate = capabilities.getVideoCapabilities().getBitrateRange().clamp(requestedVideoBitrate);
            MediaCodecInfo.EncoderCapabilities encoderCapabilities = capabilities.getEncoderCapabilities();
            if (encoderCapabilities.isBitrateModeSupported(MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR))
                bitrateMode = MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR;
            else if (encoderCapabilities.isBitrateModeSupported(MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR))
                bitrateMode = MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR;
            else throw new java.io.IOException("AVC encoder does not support bitrate-controlled export");
            vf.setInteger(MediaFormat.KEY_BIT_RATE, configuredVideoBitrate);
            vf.setInteger(MediaFormat.KEY_BITRATE_MODE, bitrateMode);
            vf.setInteger(MediaFormat.KEY_FRAME_RATE, fps); vf.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, gopSeconds);
            vf.setInteger(MediaFormat.KEY_MAX_B_FRAMES, 0);
            video.configure(vf, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            surface = video.createInputSurface(); egl = new EglTarget(surface, width, height); video.start();
            MediaFormat af = MediaFormat.createAudioFormat("audio/mp4a-latm", 48000, 2);
            af.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);
            af.setInteger(MediaFormat.KEY_BIT_RATE, audioBitrate * 1000);
            audio = MediaCodec.createEncoderByType("audio/mp4a-latm");
            audio.configure(af, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE); audio.start();
            muxer = new MediaMuxer(output, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            int videoTrack = -1, audioTrack = -1;
            long audioOffset = 0, lastProgress = android.os.SystemClock.elapsedRealtime();
            boolean videoEos = false, audioEos = false, videoDone = false, audioDone = false;
            ArrayList<Packet> pending = new ArrayList<>();
            long pendingBytes = 0;
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            byte[] scratch = new byte[65536];
            while (!videoDone || !audioDone) {
                if (cancelled) throw new InterruptedException("Export cancelled");
                long now = android.os.SystemClock.elapsedRealtime();
                if (now - lastProgress > 120000) throw new java.io.IOException("Encoder made no progress for 120 seconds");
                byte[] frame = frames.poll();
                if (frame != null) {
                    egl.draw(frame);
                    EGLExt.eglPresentationTimeANDROID(egl.display, egl.surface, encodedFrames * 1000000000L / fps);
                    if (!EGL14.eglSwapBuffers(egl.display, egl.surface)) throw new java.io.IOException("Encoder EGL swap failed");
                    ++encodedFrames; lastProgress = now;
                } else if (inputFinished && !videoEos) {
                    if (encodedFrames != expectedFrames) throw new java.io.IOException("Rendered frame count mismatch");
                    video.signalEndOfInputStream(); videoEos = true; lastProgress = now;
                }
                // Keep audio close to video so an encoder with delayed output format
                // cannot accumulate a full song in the pre-mux packet buffer.
                if (!audioEos && audioOffset / 4 <= (encodedFrames + fps) * 48000L / fps) {
                    int index = audio.dequeueInputBuffer(0);
                    if (index >= 0) {
                        ByteBuffer input = audio.getInputBuffer(index); input.clear();
                        int count = pcm.read(scratch, 0, Math.min(scratch.length, input.remaining() / 4 * 4));
                        if (count < 0) count = 0;
                        if (count % 4 != 0) throw new java.io.IOException("PCM input is not frame aligned");
                        input.put(scratch, 0, count);
                        audio.queueInputBuffer(index, 0, count, audioOffset / 4 * 1000000L / 48000,
                            count == 0 ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
                        audioOffset += count; if (count == 0) audioEos = true;
                        lastProgress = now;
                    }
                }
                for (int stream = 0; stream < 2; ++stream) {
                    MediaCodec codec = stream == 0 ? video : audio;
                    if (stream == 0 ? videoDone : audioDone) continue;
                    int index;
                    while ((index = codec.dequeueOutputBuffer(info, 1000)) != MediaCodec.INFO_TRY_AGAIN_LATER) {
                        if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                            int track = muxer.addTrack(codec.getOutputFormat());
                            if (stream == 0) videoTrack = track; else audioTrack = track;
                            if (videoTrack >= 0 && audioTrack >= 0 && !started) {
                                muxer.start(); started = true;
                                for (Packet packet : pending) packet.write(muxer, videoTrack, audioTrack);
                                pending.clear(); pendingBytes = 0;
                            }
                        } else if (index >= 0) {
                            try {
                                if (info.size > 0 && (info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
                                    ByteBuffer bytes = codec.getOutputBuffer(index);
                                    bytes.position(info.offset); bytes.limit(info.offset + info.size);
                                    if (started) muxer.writeSampleData(stream == 0 ? videoTrack : audioTrack, bytes, info);
                                    else {
                                        pendingBytes += info.size;
                                        if (pendingBytes > 16 * 1024 * 1024) throw new java.io.IOException("Encoder output format was delayed too long");
                                        pending.add(new Packet(stream, bytes, info));
                                    }
                                }
                                if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                                    if (stream == 0) videoDone = true; else audioDone = true;
                                }
                            } finally { codec.releaseOutputBuffer(index, false); }
                        }
                        lastProgress = now;
                    }
                }
                if (frame == null) Thread.sleep(2);
            }
            if (!started) throw new java.io.IOException("Muxer did not start");
            muxer.stop(); started = false;
        } finally {
            if (video != null) video.release(); if (audio != null) audio.release();
            if (egl != null) egl.close(); if (surface != null) surface.release();
            if (muxer != null) {
                if (started) try { muxer.stop(); } catch (RuntimeException ignored) {}
                muxer.release();
            }
        }
    }
    private static final class Packet {
        final int stream; final byte[] bytes;
        final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        Packet(int stream, ByteBuffer buffer, MediaCodec.BufferInfo original) {
            this.stream = stream; bytes = new byte[original.size]; buffer.get(bytes);
            info.set(0, bytes.length, original.presentationTimeUs, original.flags);
        }
        void write(MediaMuxer muxer, int video, int audio) {
            muxer.writeSampleData(stream == 0 ? video : audio, ByteBuffer.wrap(bytes), info);
        }
    }
    private static final class EglTarget implements AutoCloseable {
        EGLDisplay display = EGL14.EGL_NO_DISPLAY;
        EGLContext context = EGL14.EGL_NO_CONTEXT;
        EGLSurface surface = EGL14.EGL_NO_SURFACE;
        int program, texture, width, height;
        ByteBuffer pixels;
        FloatBuffer vertices, coordinates;
        EglTarget(Surface input, int width, int height) throws Exception {
            this.width = width; this.height = height;
            try {
                display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
                int[] version = new int[2];
                if (!EGL14.eglInitialize(display, version, 0, version, 1)) throw new java.io.IOException("EGL initialization failed");
                EGLConfig[] configs = new EGLConfig[1]; int[] count = new int[1];
                int[] attributes = {EGL14.EGL_RED_SIZE,8,EGL14.EGL_GREEN_SIZE,8,EGL14.EGL_BLUE_SIZE,8,
                    EGL14.EGL_RENDERABLE_TYPE,EGL14.EGL_OPENGL_ES2_BIT,0x3142,1,EGL14.EGL_NONE};
                if (!EGL14.eglChooseConfig(display, attributes, 0, configs, 0, 1, count, 0) || count[0] == 0)
                    throw new java.io.IOException("Recordable EGL configuration missing");
                context = EGL14.eglCreateContext(display, configs[0], EGL14.EGL_NO_CONTEXT,
                    new int[]{EGL14.EGL_CONTEXT_CLIENT_VERSION,2,EGL14.EGL_NONE}, 0);
                surface = EGL14.eglCreateWindowSurface(display, configs[0], input, new int[]{EGL14.EGL_NONE}, 0);
                if (context == EGL14.EGL_NO_CONTEXT || surface == EGL14.EGL_NO_SURFACE
                        || !EGL14.eglMakeCurrent(display, surface, surface, context)) throw new java.io.IOException("Encoder EGL surface failed");
                int vertex = shader(GLES20.GL_VERTEX_SHADER,
                    "attribute vec2 p;attribute vec2 uv;varying vec2 v;void main(){gl_Position=vec4(p,0.,1.);v=uv;}");
                int fragment = shader(GLES20.GL_FRAGMENT_SHADER,
                    "precision mediump float;varying vec2 v;uniform sampler2D image;void main(){vec4 c=texture2D(image,v);gl_FragColor=vec4(c.rgb*c.a,1.);}");
                program = GLES20.glCreateProgram(); GLES20.glAttachShader(program, vertex); GLES20.glAttachShader(program, fragment);
                GLES20.glLinkProgram(program); GLES20.glDeleteShader(vertex); GLES20.glDeleteShader(fragment);
                int[] linked = new int[1]; GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linked, 0);
                if (linked[0] == 0) throw new java.io.IOException(GLES20.glGetProgramInfoLog(program));
                int[] textures = new int[1]; GLES20.glGenTextures(1, textures, 0); texture = textures[0];
                GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
                GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
                GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, width, height, 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
                pixels = ByteBuffer.allocateDirect(width * height * 4);
                vertices = buffer(new float[]{-1,-1, 1,-1, -1,1, 1,1});
                coordinates = buffer(new float[]{0,1, 1,1, 0,0, 1,0});
            } catch (Exception failure) { close(); throw failure; }
        }
        private static FloatBuffer buffer(float[] values) {
            FloatBuffer result = ByteBuffer.allocateDirect(values.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            result.put(values).position(0); return result;
        }
        private static int shader(int type, String source) throws Exception {
            int shader = GLES20.glCreateShader(type); GLES20.glShaderSource(shader, source); GLES20.glCompileShader(shader);
            int[] compiled = new int[1]; GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0);
            if (compiled[0] == 0) { String error = GLES20.glGetShaderInfoLog(shader); GLES20.glDeleteShader(shader); throw new java.io.IOException(error); }
            return shader;
        }
        void draw(byte[] rgba) throws Exception {
            pixels.clear(); pixels.put(rgba).position(0);
            GLES20.glViewport(0,0,width,height); GLES20.glUseProgram(program);
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0); GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture);
            GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D,0,0,0,width,height,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,pixels);
            int p = GLES20.glGetAttribLocation(program,"p"), uv = GLES20.glGetAttribLocation(program,"uv");
            GLES20.glEnableVertexAttribArray(p); GLES20.glEnableVertexAttribArray(uv);
            GLES20.glVertexAttribPointer(p,2,GLES20.GL_FLOAT,false,0,vertices);
            GLES20.glVertexAttribPointer(uv,2,GLES20.GL_FLOAT,false,0,coordinates);
            GLES20.glUniform1i(GLES20.glGetUniformLocation(program,"image"),0);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP,0,4);
            if (GLES20.glGetError() != GLES20.GL_NO_ERROR) throw new java.io.IOException("Chart frame upload failed");
        }
        public void close() {
            if (display == EGL14.EGL_NO_DISPLAY) return;
            if (program != 0) GLES20.glDeleteProgram(program);
            if (texture != 0) GLES20.glDeleteTextures(1,new int[]{texture},0);
            EGL14.eglMakeCurrent(display,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_CONTEXT);
            if (surface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display,surface);
            if (context != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(display,context);
            EGL14.eglReleaseThread(); EGL14.eglTerminate(display); display = EGL14.EGL_NO_DISPLAY;
        }
    }
}
