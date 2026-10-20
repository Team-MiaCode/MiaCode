// Probe 2: long-uptime behaviour of BASS position syncs.
//  T7  Does BASS_ChannelSetSync(POS) validate the position against the mixer length? (far future)
//  T8  Decode-mode NONSTOP mixer pulled past 2^32 bytes: do POS syncs armed across / beyond
//      the 32-bit boundary fire, and does GetPosition stay monotonic?
//  T9  Playback mixer: can its position be moved near 2^32 (BASS_ChannelSetPosition)? If so,
//      does a POS|MIXTIME sync armed across the boundary fire in real playback?
#include "bass.h"
#include "bassmix.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

static void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

struct Fire {
    std::atomic<int> count{0};
    std::atomic<QWORD> pos{0};
    QWORD target = 0;
    const char* label = "";
};

static void CALLBACK onFire(HSYNC, DWORD channel, DWORD, void* user)
{
    auto* f = static_cast<Fire*>(user);
    f->count.fetch_add(1);
    f->pos.store(BASS_ChannelGetPosition(channel, BASS_POS_BYTE | BASS_POS_DECODE));
}

static void report(const Fire& f, HSYNC h, int err)
{
    std::printf("  %-40s handle=%u err=%d target=%llu fired=%d cb_pos=%llu delta=%lld bytes\n",
        f.label, h, err, (unsigned long long)f.target, f.count.load(), (unsigned long long)f.pos.load(),
        f.count.load() ? (long long)f.pos.load() - (long long)f.target : 0LL);
}

int main(int argc, char** argv)
{
    const bool skipDecode = argc > 1 && std::atoi(argv[1]) == 1;
    if (!BASS_Init(-1, 48000, 0, nullptr, nullptr)) { std::printf("init failed %d\n", BASS_ErrorGetCode()); return 1; }

    // ---- T7 ----------------------------------------------------------------
    {
        HSTREAM m = BASS_Mixer_StreamCreate(48000, 2, BASS_SAMPLE_FLOAT | BASS_MIXER_NONSTOP | BASS_MIXER_POSEX);
        BASS_ChannelSetAttribute(m, BASS_ATTRIB_BUFFER, 0.0f);
        BASS_ChannelPlay(m, FALSE);
        sleepMs(100);
        const QWORD len = BASS_ChannelGetLength(m, BASS_POS_BYTE);
        const int lenErr = BASS_ErrorGetCode();
        const QWORD pos = BASS_ChannelGetPosition(m, BASS_POS_BYTE | BASS_POS_DECODE);
        std::printf("T7 mixer length=%lld (err=%d) pos=%llu\n", (long long)len, lenErr, (unsigned long long)pos);
        static Fire far; far.label = "T7 sync at pos + 1 hour"; far.target = pos + (QWORD)3600 * 48000 * 8;
        HSYNC h = BASS_ChannelSetSync(m, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, far.target, onFire, &far);
        report(far, h, BASS_ErrorGetCode());
        static Fire huge; huge.label = "T7 sync at 2^33"; huge.target = (QWORD)1 << 33;
        h = BASS_ChannelSetSync(m, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, huge.target, onFire, &huge);
        report(huge, h, BASS_ErrorGetCode());
        BASS_StreamFree(m);
    }

    // ---- T8 ----------------------------------------------------------------
    if (!skipDecode) {
        HSTREAM d = BASS_Mixer_StreamCreate(48000, 2, BASS_SAMPLE_FLOAT | BASS_MIXER_NONSTOP | BASS_STREAM_DECODE);
        if (!d) { std::printf("decode mixer failed %d\n", BASS_ErrorGetCode()); return 1; }
        const QWORD boundary = (QWORD)1 << 32;
        static Fire before, at, after, afterFar;
        before.label = "T8 sync at 2^32 - 1MB"; before.target = boundary - (1 << 20);
        at.label = "T8 sync at 2^32 + 8 bytes"; at.target = boundary + 8;
        after.label = "T8 sync at 2^32 + 1MB"; after.target = boundary + (1 << 20);
        afterFar.label = "T8 sync at 2^32 + 64MB"; afterFar.target = boundary + ((QWORD)64 << 20);
        HSYNC h1 = BASS_ChannelSetSync(d, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, before.target, onFire, &before);
        HSYNC h2 = BASS_ChannelSetSync(d, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, at.target, onFire, &at);
        HSYNC h3 = BASS_ChannelSetSync(d, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, after.target, onFire, &after);
        HSYNC h4 = BASS_ChannelSetSync(d, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, afterFar.target, onFire, &afterFar);
        std::printf("T8 handles %u %u %u %u\n", h1, h2, h3, h4);
        std::vector<float> buf((4 << 20) / sizeof(float));
        QWORD pulled = 0, lastPos = 0; bool monotonic = true; int regressions = 0;
        const QWORD goal = boundary + ((QWORD)80 << 20);
        const auto t0 = std::chrono::steady_clock::now();
        while (pulled < goal) {
            const DWORD got = BASS_ChannelGetData(d, buf.data(), (DWORD)(buf.size() * sizeof(float)));
            if (got == (DWORD)-1) { std::printf("GetData error %d at %llu\n", BASS_ErrorGetCode(), (unsigned long long)pulled); break; }
            pulled += got;
            const QWORD p = BASS_ChannelGetPosition(d, BASS_POS_BYTE | BASS_POS_DECODE);
            if (p < lastPos) { monotonic = false; if (regressions++ < 3) std::printf("  position regressed: %llu -> %llu after pulling %llu\n", (unsigned long long)lastPos, (unsigned long long)p, (unsigned long long)pulled); }
            lastPos = p;
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("T8 pulled=%llu bytes in %.1fs, final GetPosition=%llu monotonic=%d\n",
            (unsigned long long)pulled, secs, (unsigned long long)lastPos, monotonic ? 1 : 0);
        report(before, h1, 0); report(at, h2, 0); report(after, h3, 0); report(afterFar, h4, 0);
        // Now a sync armed AFTER crossing, from the position we are at (mimics a fresh anchor post-wrap)
        static Fire postAnchor; postAnchor.label = "T8 armed after crossing, +1MB"; postAnchor.target = lastPos + (1 << 20);
        HSYNC h5 = BASS_ChannelSetSync(d, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, postAnchor.target, onFire, &postAnchor);
        for (int i = 0; i < 4; ++i) BASS_ChannelGetData(d, buf.data(), (DWORD)(buf.size() * sizeof(float)));
        report(postAnchor, h5, 0);
        BASS_StreamFree(d);
    }

    // ---- T9 ----------------------------------------------------------------
    {
        HSTREAM m = BASS_Mixer_StreamCreate(48000, 2, BASS_SAMPLE_FLOAT | BASS_MIXER_NONSTOP | BASS_MIXER_POSEX);
        BASS_ChannelSetAttribute(m, BASS_ATTRIB_BUFFER, 0.0f);
        BASS_ChannelSetAttribute(m, BASS_ATTRIB_MIXER_THREADS, 4.0f);
        BASS_ChannelPlay(m, FALSE);
        sleepMs(100);
        const QWORD boundary = (QWORD)1 << 32;
        const QWORD seekTo = boundary - (QWORD)48000 * 8 * 1; // 1s before the boundary
        const BOOL ok = BASS_ChannelSetPosition(m, seekTo, BASS_POS_BYTE);
        const int err = BASS_ErrorGetCode();
        sleepMs(50);
        const QWORD posNow = BASS_ChannelGetPosition(m, BASS_POS_BYTE | BASS_POS_DECODE);
        std::printf("T9 BASS_ChannelSetPosition(mixer, 2^32-1s) ok=%d err=%d pos_now=%llu\n", ok, err, (unsigned long long)posNow);
        if (ok && posNow >= seekTo && posNow < boundary) {
            static Fire preB, postB, postB2;
            preB.label = "T9 sync at 2^32 - 0.5s (before boundary)"; preB.target = boundary - (QWORD)48000 * 8 / 2;
            postB.label = "T9 sync at 2^32 + 0.5s (after boundary)"; postB.target = boundary + (QWORD)48000 * 8 / 2;
            postB2.label = "T9 sync at 2^32 + 1.5s"; postB2.target = boundary + (QWORD)48000 * 8 * 3 / 2;
            HSYNC a = BASS_ChannelSetSync(m, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, preB.target, onFire, &preB);
            HSYNC b = BASS_ChannelSetSync(m, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, postB.target, onFire, &postB);
            HSYNC c = BASS_ChannelSetSync(m, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, postB2.target, onFire, &postB2);
            QWORD last = posNow; bool mono = true;
            for (int i = 0; i < 30; ++i) { sleepMs(100); const QWORD p = BASS_ChannelGetPosition(m, BASS_POS_BYTE | BASS_POS_DECODE); if (p < last) { mono = false; std::printf("  playback mixer position regressed %llu -> %llu\n", (unsigned long long)last, (unsigned long long)p); } last = p; }
            std::printf("T9 after 3s: pos=%llu monotonic=%d\n", (unsigned long long)last, mono ? 1 : 0);
            report(preB, a, 0); report(postB, b, 0); report(postB2, c, 0);
            static Fire fresh; fresh.label = "T9 armed post-boundary, +0.3s"; fresh.target = last + (QWORD)48000 * 8 * 3 / 10;
            HSYNC f = BASS_ChannelSetSync(m, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, fresh.target, onFire, &fresh);
            sleepMs(600);
            report(fresh, f, 0);
        }
        BASS_StreamFree(m);
    }
    BASS_Free();
    return 0;
}
