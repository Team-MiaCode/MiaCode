// Empirical probe of BASS 2.4 position-sync semantics on the exact master-mixer
// configuration MiaCode uses (float stereo, NONSTOP|POSEX, ATTRIB_BUFFER=0,
// MIXER_THREADS=4, device -1). Answers:
//  T1  Does a POS|MIXTIME|ONETIME sync armed exactly AT the current decode position fire?
//  T2  Does one armed BEHIND the current decode position fire?
//  T3  Inside a MIXTIME callback, what does GetPosition(DECODE) report vs the sync's target?
//  T4  Chain: a callback arms the next sync at +delta; which deltas survive a 40-link chain?
//  T5  Chain with a deliberately slow callback (worker-style stall) - do syncs get lost?
//  T6  Mix block size as seen by a DSP on the mixer.
#include "bass.h"
#include "bassmix.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

static HSTREAM g_mixer = 0;
static std::atomic<int> g_fired{0};
static std::atomic<QWORD> g_lastFirePos{0};
static std::atomic<QWORD> g_lastTarget{0};

struct ChainState {
    std::atomic<int> links{0};
    std::atomic<int> maxLinks{0};
    std::atomic<int> setFailures{0};
    QWORD deltaBytes = 0;
    int sleepMs = 0;
    std::atomic<QWORD> lastTarget{0};
    std::atomic<QWORD> lastSeenPos{0};
    std::atomic<QWORD> maxLagBytes{0}; // (pos inside callback) - target
    std::atomic<HSYNC> armed{0};
};

static void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static QWORD decodePos() { return BASS_ChannelGetPosition(g_mixer, BASS_POS_BYTE | BASS_POS_DECODE); }

static double bytesToMs(QWORD b) { return BASS_ChannelBytes2Seconds(g_mixer, b) * 1000.0; }

// ---- T1/T2/T3 -------------------------------------------------------------
static void CALLBACK onceSync(HSYNC, DWORD, DWORD, void* user)
{
    g_fired.fetch_add(1);
    g_lastFirePos.store(decodePos());
    g_lastTarget.store(*static_cast<QWORD*>(user));
}

static void runOnce(const char* label, long long offsetBytes, int waitMs)
{
    static QWORD target;
    g_fired.store(0);
    const QWORD before = decodePos();
    target = before + offsetBytes;
    const HSYNC h = BASS_ChannelSetSync(
        g_mixer, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, target, onceSync, &target);
    const int err = BASS_ErrorGetCode();
    const QWORD after = decodePos();
    sleepMs(waitMs);
    const int fired = g_fired.load();
    std::printf("%-44s handle=%u err=%d pos_before=%llu target=%llu pos_after_set=%llu (target-before=%+.3fms) fired=%d",
        label, h, err, (unsigned long long)before, (unsigned long long)target,
        (unsigned long long)after, (double)offsetBytes / 8.0 / 48.0, fired);
    if (fired) {
        const QWORD fp = g_lastFirePos.load();
        std::printf(" cb_pos=%llu cb_pos-target=%+.3fms", (unsigned long long)fp,
            (double)((long long)fp - (long long)target) / 8.0 / 48.0);
    }
    std::printf("\n");
    if (h != 0 && !fired) {
        BASS_ChannelRemoveSync(g_mixer, h);
    }
}

// ---- T4/T5 chain ------------------------------------------------------------
static void CALLBACK chainSync(HSYNC handle, DWORD, DWORD, void* user)
{
    auto* st = static_cast<ChainState*>(user);
    const QWORD pos = decodePos();
    const QWORD target = st->lastTarget.load();
    st->lastSeenPos.store(pos);
    if (pos > target) {
        QWORD lag = pos - target;
        QWORD cur = st->maxLagBytes.load();
        while (lag > cur && !st->maxLagBytes.compare_exchange_weak(cur, lag)) {}
    }
    const int links = st->links.fetch_add(1) + 1;
    if (st->sleepMs > 0) {
        sleepMs(st->sleepMs);
    }
    if (links >= st->maxLinks.load()) {
        st->armed.store(0);
        return;
    }
    const QWORD next = target + st->deltaBytes;
    st->lastTarget.store(next);
    const HSYNC h = BASS_ChannelSetSync(
        g_mixer, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, next, chainSync, st);
    if (h == 0) {
        st->setFailures.fetch_add(1);
    }
    st->armed.store(h);
}

static void runChain(const char* label, double deltaMs, int links, int cbSleepMs, int waitMs)
{
    ChainState st;
    st.deltaBytes = BASS_ChannelSeconds2Bytes(g_mixer, deltaMs / 1000.0);
    st.sleepMs = cbSleepMs;
    st.maxLinks.store(links);
    const QWORD start = decodePos() + BASS_ChannelSeconds2Bytes(g_mixer, 0.050);
    st.lastTarget.store(start);
    const HSYNC h = BASS_ChannelSetSync(
        g_mixer, BASS_SYNC_POS | BASS_SYNC_MIXTIME | BASS_SYNC_ONETIME, start, chainSync, &st);
    st.armed.store(h);
    sleepMs(waitMs);
    const int got = st.links.load();
    std::printf("%-44s delta=%.2fms links_expected=%d fired=%d set_failures=%d max_cb_lag=%.3fms %s\n",
        label, deltaMs, links, got, st.setFailures.load(), bytesToMs(st.maxLagBytes.load()),
        got == links ? "OK" : "*** CHAIN BROKE ***");
    if (st.armed.load() != 0) {
        BASS_ChannelRemoveSync(g_mixer, st.armed.load());
    }
}

// ---- T6 DSP block size ------------------------------------------------------
static std::atomic<DWORD> g_minBlock{0xffffffff};
static std::atomic<DWORD> g_maxBlock{0};
static std::atomic<int> g_blocks{0};
static void CALLBACK dspProc(HDSP, DWORD, void*, DWORD length, void*)
{
    g_blocks.fetch_add(1);
    DWORD cur = g_minBlock.load();
    while (length < cur && !g_minBlock.compare_exchange_weak(cur, length)) {}
    cur = g_maxBlock.load();
    while (length > cur && !g_maxBlock.compare_exchange_weak(cur, length)) {}
}

int main(int argc, char** argv)
{
    const int threads = argc > 1 ? std::atoi(argv[1]) : 4;
    const float bufferSec = argc > 2 ? (float)std::atof(argv[2]) : 0.0f;
    std::printf("BASS version %08x, bassmix %08x\n", BASS_GetVersion(), BASS_Mixer_GetVersion());
    if (!BASS_Init(-1, 48000, 0, nullptr, nullptr)) {
        std::printf("BASS_Init failed err=%d\n", BASS_ErrorGetCode());
        return 1;
    }
    BASS_INFO info{};
    BASS_GetInfo(&info);
    std::printf("device freq=%u minbuf=%u latency=%u config_buffer=%u update_period=%u dev_buffer=%u dev_period=%d\n",
        info.freq, info.minbuf, info.latency, BASS_GetConfig(BASS_CONFIG_BUFFER),
        BASS_GetConfig(BASS_CONFIG_UPDATEPERIOD), BASS_GetConfig(BASS_CONFIG_DEV_BUFFER),
        (int)BASS_GetConfig(BASS_CONFIG_DEV_PERIOD));
    g_mixer = BASS_Mixer_StreamCreate(48000, 2, BASS_SAMPLE_FLOAT | BASS_MIXER_NONSTOP | BASS_MIXER_POSEX);
    if (!g_mixer) { std::printf("mixer create failed err=%d\n", BASS_ErrorGetCode()); return 1; }
    BASS_ChannelSetAttribute(g_mixer, BASS_ATTRIB_BUFFER, bufferSec);
    BASS_ChannelSetAttribute(g_mixer, BASS_ATTRIB_MIXER_THREADS, (float)threads);
    float eb = -1, et = -1;
    BASS_ChannelGetAttribute(g_mixer, BASS_ATTRIB_BUFFER, &eb);
    BASS_ChannelGetAttribute(g_mixer, BASS_ATTRIB_MIXER_THREADS, &et);
    std::printf("mixer buffer=%.3fs threads=%.0f\n", eb, et);
    BASS_ChannelSetDSPEx(g_mixer, dspProc, nullptr, 0, BASS_DSP_READONLY);
    if (!BASS_ChannelPlay(g_mixer, FALSE)) { std::printf("play failed err=%d\n", BASS_ErrorGetCode()); return 1; }
    sleepMs(500);
    std::printf("T6 mix blocks: count=%d min=%u bytes (%.2fms) max=%u bytes (%.2fms)\n",
        g_blocks.load(), g_minBlock.load(), bytesToMs(g_minBlock.load()), g_maxBlock.load(), bytesToMs(g_maxBlock.load()));
    // Decode position advance sanity
    const QWORD p0 = decodePos(); sleepMs(200); const QWORD p1 = decodePos();
    std::printf("decode pos advance over 200ms: %.2fms\n", bytesToMs(p1 - p0));

    std::printf("\n== T1/T2/T3: single syncs relative to current decode position ==\n");
    runOnce("T1 target == pos", 0, 300);
    runOnce("T1b target == pos (repeat)", 0, 300);
    runOnce("T2 target = pos - 1 sample", -8, 300);
    runOnce("T2b target = pos - 1ms", -8 * 48, 300);
    runOnce("T2c target = pos - 50ms", -(long long)(8 * 48 * 50), 300);
    runOnce("T3 target = pos + 1 sample", 8, 300);
    runOnce("T3b target = pos + 2ms", 8 * 48 * 2, 300);
    runOnce("T3c target = pos + 30ms", 8 * 48 * 30, 300);
    runOnce("T3d target = pos + 30ms (repeat)", 8 * 48 * 30, 300);

    std::printf("\n== T4: chains armed from inside the callback ==\n");
    runChain("T4a delta=1 sample", 1.0 / 48.0, 40, 0, 1500);
    runChain("T4b delta=1ms", 1.0, 40, 0, 1500);
    runChain("T4c delta=3ms", 3.0, 40, 0, 1500);
    runChain("T4d delta=5ms", 5.0, 40, 0, 1500);
    runChain("T4e delta=8ms", 8.0, 40, 0, 1500);
    runChain("T4f delta=12ms", 12.0, 40, 0, 1500);
    runChain("T4g delta=16.7ms", 16.6667, 40, 0, 1500);
    runChain("T4h delta=25ms", 25.0, 40, 0, 2000);
    runChain("T4i delta=100ms", 100.0, 20, 0, 3000);

    std::printf("\n== T5: chains whose callback stalls (simulated slow work) ==\n");
    runChain("T5a delta=16.7ms cb_sleep=5ms", 16.6667, 30, 5, 1500);
    runChain("T5b delta=16.7ms cb_sleep=15ms", 16.6667, 30, 15, 2000);
    runChain("T5c delta=16.7ms cb_sleep=40ms", 16.6667, 20, 40, 3000);
    runChain("T5d delta=100ms cb_sleep=40ms", 100.0, 10, 40, 3000);

    std::printf("\n== T6 again after tests ==\n");
    std::printf("blocks=%d min=%.2fms max=%.2fms\n", g_blocks.load(), bytesToMs(g_minBlock.load()), bytesToMs(g_maxBlock.load()));
    BASS_Free();
    return 0;
}
