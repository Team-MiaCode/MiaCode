package org.miacode.android;

import org.qtproject.qt.android.bindings.QtActivity;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.provider.DocumentsContract;
import android.database.Cursor;
import android.util.Base64;
import org.json.JSONObject;
import org.json.JSONArray;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public final class MiaCodeActivity extends QtActivity {
    private static final int PICK_DOCUMENT = 4101;
    private static final long CHART_LIMIT = 16L * 1024 * 1024;
    private static final long ASSET_LIMIT = 512L * 1024 * 1024;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final AndroidUiFiles uiFiles = new AndroidUiFiles(this, io);
    private String pendingKind = "";
    private String pendingPayload = "";
    private volatile boolean foregroundProbeActive;
    private volatile boolean uiResumed;
    public static native void deliverResult(String json);
    public boolean isUiResumed() { return uiResumed; }
    @Override protected void onResume() {
        uiResumed = true;
        super.onResume();
    }
    public void requestUiFile(String payload) { uiFiles.request(payload); }
    boolean hasPendingFileOperation() { return !pendingKind.isEmpty(); }

    public void dispatch(String kind, String uri, String payload) {
        runOnUiThread(() -> {
            if (!pendingKind.isEmpty() || uiFiles.busy()) {
                try { deliverResult(new JSONObject().put("kind", kind).put("ok", false).put("error", "已有文件操作正在进行").toString()); }
                catch (Exception ignored) { }
                return;
            }
            pendingKind = kind;
            pendingPayload = payload;
            try {
                if (kind.equals("save")) {
                    io.execute(() -> saveChart(Uri.parse(uri), payload));
                } else if (kind.equals("share")) {
                    Intent share = new Intent(Intent.ACTION_SEND).setType("text/plain")
                        .putExtra(Intent.EXTRA_STREAM, Uri.parse(uri))
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    startActivity(Intent.createChooser(share, "分享谱面"));
                    finishResult(kind, new JSONObject().put("ok", true));
                } else if (kind.equals("probe")) {
                    startProbe(payload.equals("background"));
                } else if (kind.equals("openFolder")) {
                    Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                            | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                    startActivityForResult(picker, PICK_DOCUMENT);
                } else {
                    final boolean create = kind.equals("saveAs") || kind.equals("probeReport");
                    Intent picker = new Intent(create ? Intent.ACTION_CREATE_DOCUMENT : Intent.ACTION_OPEN_DOCUMENT);
                    picker.addCategory(Intent.CATEGORY_OPENABLE);
                    picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                        | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                    picker.setType(kind.equals("asset") ? mimeFor(payload) : kind.equals("probeReport") ? "application/zip" : "text/plain");
                    if (create) picker.putExtra(Intent.EXTRA_TITLE, kind.equals("probeReport") ? "miacode-p0-probe.zip" : "maidata.txt");
                    startActivityForResult(picker, PICK_DOCUMENT);
                }
            } catch (Exception error) { fail(kind, error.toString()); }
        });
    }

    private static String mimeFor(String kind) {
        if (kind.equals("audio")) return "audio/*";
        if (kind.equals("image")) return "image/*";
        if (kind.equals("video")) return "video/*";
        return "*/*";
    }

    @Override public void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (uiFiles.onResult(request, result, data)) return;
        if (request != PICK_DOCUMENT || pendingKind.isEmpty()) return;
        String kind = pendingKind;
        String payload = pendingPayload;
        if (result != Activity.RESULT_OK || data == null || data.getData() == null) {
            try { finishResult(kind, new JSONObject().put("ok", false).put("cancelled", true)); }
            catch (Exception error) { fail(kind, error.toString()); }
            return;
        }
        Uri uri = data.getData();
        try {
            int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            getContentResolver().takePersistableUriPermission(uri, flags);
        } catch (SecurityException ignored) {
            // Some providers only grant this session. Internal recovery still works;
            // a later failed save asks the user to select a destination again.
        }
        io.execute(() -> {
            if (kind.equals("saveAs")) saveChart(uri, payload);
            else if (kind.equals("probeReport")) exportProbe(uri);
            else if (kind.equals("openFolder")) readProjectFolder(uri);
            else readDocument(kind, uri, payload);
        });
    }

    private void readDocument(String kind, Uri uri, String assetKind) {
        File destination = null;
        try {
            String name = "document";
            try (Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
            }
            JSONObject result = new JSONObject().put("ok", true).put("uri", uri.toString());
            if (kind.equals("asset")) {
                String suffix = "";
                int dot = name == null ? -1 : name.lastIndexOf('.');
                if (dot >= 0) suffix = name.substring(dot).toLowerCase(java.util.Locale.ROOT);
                if (!suffix.matches("\\.[a-z0-9]{1,8}")) suffix = "";
                if (assetKind.equals("font") && !suffix.equals(".ttf") && !suffix.equals(".otf"))
                    throw new IllegalArgumentException("字体仅接受 TTF / OTF 文件");
                File directory = new File(getFilesDir(), "imports");
                if (!directory.isDirectory() && !directory.mkdirs()) throw new java.io.IOException("无法创建素材目录");
                destination = new File(directory, UUID.randomUUID().toString() + suffix);
                try (InputStream input = getContentResolver().openInputStream(uri); FileOutputStream output = new FileOutputStream(destination)) {
                    copyBounded(input, output, ASSET_LIMIT);
                    output.getFD().sync();
                }
                result.put("asset", new JSONObject().put("kind", assetKind).put("name", name)
                    .put("path", destination.getAbsolutePath()).put("uri", uri.toString()));
            } else {
                try (InputStream input = getContentResolver().openInputStream(uri); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                    copyBounded(input, output, CHART_LIMIT);
                    byte[] bytes = output.toByteArray();
                    File directory = new File(getFilesDir(), "projects/" + UUID.randomUUID());
                    if (!directory.mkdirs()) throw new java.io.IOException("无法创建谱面工作目录");
                    destination = new File(directory, "maidata.txt");
                    try (FileOutputStream working = new FileOutputStream(destination)) {
                        working.write(bytes); working.getFD().sync();
                    }
                    result.put("data", Base64.encodeToString(bytes, Base64.NO_WRAP)).put("path", destination.getAbsolutePath());
                }
            }
            finishResult(kind, result);
        } catch (Exception error) {
            if (destination != null) destination.delete();
            fail(kind, error.toString());
        }
    }

    static void copyBounded(InputStream input, OutputStream output, long limit) throws Exception {
        if (input == null || output == null) throw new java.io.IOException("文件提供方未返回数据流");
        byte[] buffer = new byte[65536];
        long total = 0;
        int count;
        while ((count = input.read(buffer)) != -1) {
            total += count;
            if (total > limit) throw new java.io.IOException("文件超过导入大小限制");
            output.write(buffer, 0, count);
        }
    }

    File importUiDirectory(Uri tree) throws Exception {
        File root = new File(getFilesDir(), "ui-files/imports/" + UUID.randomUUID());
        if (!root.mkdirs()) throw new java.io.IOException("无法创建工程工作目录");
        copyProjectDirectory(tree, DocumentsContract.getTreeDocumentId(tree), root, "", new JSONArray(), new JSONObject(), new long[]{0}, 0);
        return root;
    }

    private void readProjectFolder(Uri tree) {
        try {
            String id = DocumentsContract.getTreeDocumentId(tree);
            File root = new File(getFilesDir(), "projects/" + UUID.randomUUID());
            if (!root.mkdirs()) throw new java.io.IOException("无法创建工程工作目录");
            JSONArray assets = new JSONArray();
            JSONObject chart = new JSONObject();
            copyProjectDirectory(tree, id, root, "", assets, chart, new long[]{0}, 0);
            if (!chart.has("data")) throw new java.io.IOException("所选文件夹根目录没有 maidata.txt");
            chart.put("ok", true).put("assets", assets);
            finishResult("openFolder", chart);
        } catch (Exception error) { fail("openFolder", error.toString()); }
    }

    private void copyProjectDirectory(Uri tree, String id, File destination, String relative,
            JSONArray assets, JSONObject chart, long[] total, int depth) throws Exception {
        if (depth > 8) throw new java.io.IOException("工程目录嵌套超过 8 层");
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
        try (Cursor cursor = getContentResolver().query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null)) {
            if (cursor == null) throw new java.io.IOException("无法枚举工程目录");
            while (cursor.moveToNext()) {
                String childId = cursor.getString(0), name = cursor.getString(1), mime = cursor.getString(2);
                if (name == null || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\"))
                    throw new java.io.IOException("工程文件名无效");
                File outputFile = new File(destination, name);
                if (mime.equals(DocumentsContract.Document.MIME_TYPE_DIR)) {
                    if (!outputFile.mkdirs()) throw new java.io.IOException("无法创建工程子目录");
                    copyProjectDirectory(tree, childId, outputFile, relative + name + "/", assets, chart, total, depth + 1);
                    continue;
                }
                Uri source = DocumentsContract.buildDocumentUriUsingTree(tree, childId);
                try (InputStream input = getContentResolver().openInputStream(source); FileOutputStream output = new FileOutputStream(outputFile)) {
                    copyBounded(input, output, ASSET_LIMIT); output.getFD().sync();
                }
                total[0] += outputFile.length();
                if (total[0] > 1024L * 1024 * 1024) throw new java.io.IOException("工程导入总大小超过 1 GiB");
                if (depth == 0 && name.equalsIgnoreCase("maidata.txt")) {
                    if (outputFile.length() > CHART_LIMIT) throw new java.io.IOException("谱面文件超过 16 MiB");
                    chart.put("uri", source.toString()).put("data", Base64.encodeToString(
                        java.nio.file.Files.readAllBytes(outputFile.toPath()), Base64.NO_WRAP))
                        .put("path", outputFile.getAbsolutePath());
                } else {
                    String kind = mime.startsWith("audio/") ? "audio" : mime.startsWith("image/") ? "image"
                        : mime.startsWith("video/") ? "video" : name.toLowerCase(java.util.Locale.ROOT).matches(".*\\.(ttf|otf)$") ? "font" : "file";
                    assets.put(new JSONObject().put("kind", kind).put("name", relative + name)
                        .put("path", outputFile.getAbsolutePath()).put("uri", source.toString()));
                }
            }
        }
    }

    private void saveChart(Uri uri, String payload) {
        try {
            byte[] bytes = Base64.decode(payload, Base64.DEFAULT);
            try (OutputStream output = getContentResolver().openOutputStream(uri, "wt")) {
                if (output == null) throw new java.io.IOException("无法写入所选文件");
                output.write(bytes);
                output.flush();
            }
            // Providers cannot promise atomic replacement. Preserve the local
            // recovery and verify bytes before advancing the document save point.
            try (InputStream input = getContentResolver().openInputStream(uri); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                copyBounded(input, output, CHART_LIMIT);
                if (!java.util.Arrays.equals(bytes, output.toByteArray())) throw new java.io.IOException("保存后读取校验不一致");
            }
            finishResult(pendingKind, new JSONObject().put("ok", true).put("uri", uri.toString()));
        } catch (Exception error) { fail(pendingKind, error.toString()); }
    }

    private void exportProbe(Uri uri) {
        try {
            File root = new File(getFilesDir(), "probe");
            File[] files = root.listFiles();
            if (files == null || files.length == 0) throw new java.io.IOException("请先运行媒体探针");
            try (OutputStream raw = getContentResolver().openOutputStream(uri, "wt"); ZipOutputStream zip = new ZipOutputStream(raw)) {
                for (File file : files) {
                    if (!file.isFile() || file.getName().endsWith(".tmp")) continue;
                    zip.putNextEntry(new ZipEntry(file.getName()));
                    try (InputStream input = new java.io.FileInputStream(file)) { copyBounded(input, zip, ASSET_LIMIT); }
                    zip.closeEntry();
                }
            }
            finishResult("probeReport", new JSONObject().put("ok", true).put("uri", uri.toString()));
        } catch (Exception error) { fail("probeReport", error.toString()); }
    }

    private void startProbe(boolean background) throws Exception {
        final String token = UUID.randomUUID().toString();
        if (background) {
            if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)
                    != android.content.pm.PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{android.Manifest.permission.POST_NOTIFICATIONS}, 4102);
            startForegroundService(new Intent(this, ProbeExportService.class).putExtra("run", token));
            io.execute(() -> {
                File report = new File(getFilesDir(), "probe/" + token + ".json");
                try {
                    // Service uses a dedicated process. A run token prevents an
                    // earlier report being mistaken for this invocation's result.
                    long deadline = android.os.SystemClock.elapsedRealtime() + 180000;
                    while (android.os.SystemClock.elapsedRealtime() < deadline) {
                        if (report.isFile()) {
                            String text = new String(java.nio.file.Files.readAllBytes(report.toPath()), StandardCharsets.UTF_8);
                            JSONObject object = new JSONObject(text);
                            finishResult("probe", new JSONObject().put("ok", object.getBoolean("ok")).put("report", text)
                                .put("error", object.optString("error")));
                            return;
                        }
                        Thread.sleep(300);
                    }
                    fail("probe", "后台探针超时；可导出结果或重新运行");
                } catch (Exception error) { fail("probe", error.toString()); }
            });
        } else {
            foregroundProbeActive = true;
            io.execute(() -> {
                try {
                    String report = MediaPipelineProbe.run(this, () -> !foregroundProbeActive, token);
                    finishResult("probe", new JSONObject().put("ok", true).put("report", report));
                } catch (Exception error) { fail("probe", error.toString()); }
                finally { foregroundProbeActive = false; }
            });
        }
    }

    @Override protected void onPause() {
        uiResumed = false;
        super.onPause();
        foregroundProbeActive = false;
    }

    private void fail(String kind, String error) {
        try { finishResult(kind, new JSONObject().put("ok", false).put("error", error)); }
        catch (Exception impossible) { android.util.Log.e("MiaCode", "Result delivery failed", impossible); }
    }
    private void finishResult(String kind, JSONObject result) throws Exception {
        result.put("kind", kind);
        runOnUiThread(() -> {
            pendingKind = "";
            pendingPayload = "";
            deliverResult(result.toString());
        });
    }
}
