// Probe 3: the SFX voice path exactly as BassPreviewAudioBackend::Sample builds it.
//   source (memory WAV, DECODE|PRESCAN) -> resampler mixer (DECODE|FLOAT|NONSTOP)
//   -> master mixer (FLOAT|NONSTOP|POSEX, buffer 0, 4 threads, playing)
// playOneShot = SetAttribute(VOL) + BASS_Mixer_ChannelSetPosition(source,0) + clear CHAN_PAUSE.
// We retrigger many times, from the worker thread and from inside a master POS|MIXTIME sync
// callback, and measure whether audio energy actually appears on the master output each time.
#include "bass.h"
#include "bassmix.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static HSTREAM g_master = 0;
static std::atomic<float> g_peakSinceReset{0.0f};
static std::atomic<QWORD> g_firstNonSilentPos{0};

static void CALLBACK meterDsp(HDSP, DWORD, void* buffer, DWORD length, void*)
{
    const float* s = static_cast<const float*>(buffer);
    const DWORD n = length / sizeof(float);
    float peak = g_peakSinceReset.load();
    for (DWORD i = 0; i < n; ++i) {
        const float a = std::fabs(s[i]);
        if (a > peak) peak = a;
    }
    g_peakSinceReset.store(peak);
}

static std::vector<unsigned char> makeWav(int sampleRate, int channels, double seconds, double freqHz, float amp)
{
    const int frames = static_cast<int>(seconds * sampleRate);
    const int dataBytes = frames * channels * 2;
    std::vector<unsigned char> w(44 + dataBytes);
    auto put32 = [&](int off, unsigned v) { w[off] = v & 255; w[off+1] = (v >> 8) & 255; w[off+2] = (v >> 16) & 255; w[off+3] = (v >> 24) & 255; };
    auto put16 = [&](int off, unsigned v) { w[off] = v & 255; w[off+1] = (v >> 8) & 255; };
    std::memcpy(&w[0], "RIFF", 4); put32(4, 36 + dataBytes); std::memcpy(&w[8], "WAVE", 4);
    std::memcpy(&w[12], "fmt ", 4); put32(16, 16); put16(20, 1); put16(22, channels); put32(24, sampleRate);
    put32(28, sampleRate * channels * 2); put16(32, channels * 2); put16(34, 16);
    std::memcpy(&w[36], "data", 4); put32(40, dataBytes);
    for (int f = 0; f < frames; ++f) {
        // Loud from sample 0 so ramp-in behaviour is exercised; short decay.
        const double env = 1.0 - (double)f / frames;
        const short v = (short)(amp * 32767.0 * env * std::sin(2 * M_PI * freqHz * f / sampleRate));
        for (int c = 0; c < channels; ++c) put16(44 + (f * channels + c) * 2, (unsigned short)v);
    }
    return w;
}

struct Voice {
    std::vector<unsigned char> bytes;
    HSTREAM source = 0;
    HSTREAM resampler = 0;
    bool create(int srcRate)
    {
        bytes = makeWav(srcRate, 2, 0.120, 880.0, 0.8f);
        source = BASS_StreamCreateFile(BASS_FILE_MEM, bytes.data(), 0, bytes.size(), BASS_STREAM_DECODE | BASS_STREAM_PRESCAN);
        if (!source) { std::printf("source create failed %d\n", BASS_ErrorGetCode()); return false; }
        float freq = 48000; BASS_ChannelGetAttribute(source, BASS_ATTRIB_FREQ, &freq);
        resampler = BASS_Mixer_StreamCreate((DWORD)freq, 2, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_MIXER_NONSTOP);
        if (!resampler) { std::printf("resampler create failed %d\n", BASS_ErrorGetCode()); return false; }
        BASS_ChannelSetAttribute(resampler, BASS_ATTRIB_BUFFER, 0.0f); (void)BASS_ErrorGetCode();
        if (!BASS_Mixer_StreamAddChannel(resampler, source, 0)) std::printf("add src->resampler failed %d\n", BASS_ErrorGetCode());
        BASS_Mixer_ChannelFlags(source, BASS_MIXER_CHAN_PAUSE, BASS_MIXER_CHAN_PAUSE);
        if (!BASS_Mixer_StreamAddChannel(g_master, resampler, 0)) std::printf("add resampler->master failed %d\n", BASS_ErrorGetCode());
        BASS_ChannelSetPosition(source, 0, BASS_POS_BYTE);
        BASS_ChannelSetAttribute(source, BASS_ATTRIB_VOL, 1.0f);
        return true;
    }
    // Mirrors Sample::playOneShot; returns 0 on success else the BASS error.
    int playOneShot(int* seekErr, int* flagErr)
    {
        BASS_ChannelSetAttribute(source, BASS_ATTRIB_VOL, 1.0f);
        const BOOL seeked = BASS_Mixer_ChannelSetPosition(source, 0, BASS_POS_BYTE);
        *seekErr = BASS_ErrorGetCode();
        if (!seeked) return *seekErr ? *seekErr : -1;
        const DWORD flags = BASS_Mixer_ChannelFlags(source, 0, BASS_MIXER_CHAN_PAUSE);
        *flagErr = BASS_ErrorGetCode();
        if (flags == (DWORD)-1) return *flagErr ? *flagErr : -2;
        return 0;
    }
};

static Voice g_voice;
static std::atomic<int> g_cbTriggers{0};
static std::atomic<int> g_cbFailures{0};

static void CALLBACK triggerFromSync(HSYNC, DWORD, DWORD, void*)
{
    int se = 0, fe = 0;
    if (g_voice.playOneShot(&se, &fe) != 0) g_cbFailures.fetch_add(1);
    g_cbTriggers.fetch_add(1);
}

static void measure(const char* label, int reps, int gapMs, bool viaSync)
{
    int silent = 0, apiFail = 0; float minPeak = 9.0f, maxPeak = 0.0f;
    for (int i = 0; i < reps; ++i) {
        g_peakSinceReset.store(0.0f);
        int se = 0, fe = 0;
        if (viaSync) {
            const QWORD pos = BASS_ChannelGetPosition(g_master, BASS_POS_BYTE | BASS_POS_DECODE);
            const HSYNC h = BASS_ChannelSetSync(g_master, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME,
                pos + 48000 * 8 / 50 /* +20ms */, triggerFromSync, nullptr);
            if (!h) apiFail++;
        } else {
            if (g_voice.playOneShot(&se, &fe) != 0) apiFail++;
        }
        sleepMs(gapMs);
        const float p = g_peakSinceReset.load();
        if (p < 0.05f) silent++;
        if (p < minPeak) minPeak = p; if (p > maxPeak) maxPeak = p;
        const DWORD active = BASS_Mixer_ChannelIsActive(g_voice.source);
        const DWORD fl = BASS_Mixer_ChannelFlags(g_voice.source, 0, 0);
        if (p < 0.05f && silent <= 3) std::printf("    rep %d SILENT peak=%.3f active=%u flags=0x%x seekErr=%d flagErr=%d\n", i, p, active, fl, se, fe);
    }
    std::printf("%-46s reps=%d gap=%dms silent=%d api_fail=%d peak[min=%.3f max=%.3f] %s\n",
        label, reps, gapMs, silent, apiFail, minPeak, maxPeak, silent == 0 && apiFail == 0 ? "OK" : "*** PROBLEM ***");
}

int main()
{
    if (!BASS_Init(-1, 48000, 0, nullptr, nullptr)) { std::printf("init failed %d\n", BASS_ErrorGetCode()); return 1; }
    g_master = BASS_Mixer_StreamCreate(48000, 2, BASS_SAMPLE_FLOAT | BASS_MIXER_NONSTOP | BASS_MIXER_POSEX);
    BASS_ChannelSetAttribute(g_master, BASS_ATTRIB_BUFFER, 0.0f);
    BASS_ChannelSetAttribute(g_master, BASS_ATTRIB_MIXER_THREADS, 4.0f);
    BASS_ChannelSetDSPEx(g_master, meterDsp, nullptr, 0, BASS_DSP_READONLY);
    // Keep the master nearly silent for the ears: master volume very low, but the DSP meters the mix.
    BASS_ChannelSetAttribute(g_master, BASS_ATTRIB_VOL, 0.02f);
    BASS_ChannelPlay(g_master, FALSE);
    sleepMs(200);
    std::printf("== voice at 44100 Hz source (resampled to 48k master) ==\n");
    if (!g_voice.create(44100)) return 1;
    sleepMs(100);
    measure("V1 worker-thread retrigger, 300ms gaps", 30, 300, false);
    measure("V2 worker-thread retrigger, 60ms gaps (overlap)", 30, 60, false);
    measure("V3 sync-callback retrigger, 300ms gaps", 30, 300, true);
    measure("V4 sync-callback retrigger, 40ms gaps (overlap)", 40, 40, true);
    std::printf("V5 idle 20s with the voice ended+paused, then retrigger...\n");
    sleepMs(20000);
    measure("V5 after 20s idle, worker retrigger", 5, 300, false);
    measure("V6 after idle, sync-callback retrigger", 5, 300, true);
    // stop()/pause semantics used by stopSfxVoices/stopAllSamples: PAUSE flag + Mixer_ChannelSetPosition(0)
    BASS_Mixer_ChannelFlags(g_voice.source, BASS_MIXER_CHAN_PAUSE, BASS_MIXER_CHAN_PAUSE);
    BASS_Mixer_ChannelSetPosition(g_voice.source, 0, BASS_POS_BYTE);
    sleepMs(100);
    measure("V7 after stop(): retrigger", 5, 300, false);
    // Retrigger while the master mixer is being fed a tempo-like heavy load: 4 more voices
    std::vector<Voice> extra(6);
    for (auto& v : extra) v.create(44100);
    measure("V8 with 6 extra sources attached, sync retrigger", 20, 100, true);
    std::printf("callback triggers=%d callback api failures=%d\n", g_cbTriggers.load(), g_cbFailures.load());
    BASS_Free();
    return 0;
}
