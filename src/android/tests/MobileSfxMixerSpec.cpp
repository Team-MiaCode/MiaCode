#include "android/MobileSfxMixer.h"
#include "common/PreviewSfxAssets.h"
#include "common/IntroConfig.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <bit>
#include <cstdio>
#include <limits>

using namespace miacode::android;
namespace {
int failures = 0;
void check(bool ok, const char* name) { if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", name); } }
bool near(float actual, float expected) { return qAbs(actual - expected) < 0.00001f; }
MobileSfxClip ramp() {
    MobileSfxClip clip; clip.sampleRate = 100; clip.channels = 1; clip.samples.resize(600);
    for (int i = 0; i < clip.samples.size(); ++i) clip.samples[i] = i / 1000.0f;
    return clip;
}
QByteArray wave(bool floating, float value) {
    QByteArray result; QDataStream out(&result, QIODevice::WriteOnly); out.setByteOrder(QDataStream::LittleEndian);
    const int bytes = floating ? 4 : 2;
    out.writeRawData("RIFF", 4); out << quint32(36 + bytes); out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(floating ? 3 : 1) << quint16(1) << quint32(48000)
        << quint32(48000 * bytes) << quint16(bytes) << quint16(bytes * 8);
    out.writeRawData("data", 4); out << quint32(bytes);
    if (floating) out << std::bit_cast<quint32>(value); else out << qint16(value * 32768);
    return result;
}
PreviewAudioSettings fullLevels() {
    PreviewAudioSettings levels;
    levels.globalVolume = levels.answerVolume = levels.tapVolume = levels.exVolume = levels.touchVolume = levels.breakSlideVolume = 1;
    return levels;
}
QVector<float> render(MobileSfxMixer& mixer, int frames) {
    QVector<float> samples(frames); mixer.render(QSpan<float>(samples.data(), samples.size()), 100, 1); return samples;
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    MobileSfxClip decoded; QString error;
    check(decodeMobileSfxWav(wave(false, -.5f), &decoded, &error) && near(decoded.samples[0], -.5f), "signed PCM16 decode");
    check(decodeMobileSfxWav(wave(true, .375f), &decoded, &error) && near(decoded.samples[0], .375f), "float32 decode");
    check(!decodeMobileSfxWav(wave(true, std::numeric_limits<float>::quiet_NaN()), &decoded, &error), "non-finite float rejected");
    auto truncated = wave(false, .5f); truncated.chop(1);
    check(!decodeMobileSfxWav(truncated, &decoded, &error), "truncated PCM rejected");
    for (const auto* kind : {"answer", "judge", "judge_break", "slide", "break", "break_slide_start",
            "break_slide_finish", "break_slide_tail_break", "judge_break_slide", "ex", "touch", "touchhold", "firework", "clock", "track_start"}) {
        QFile file(miacode::preview_sfx::assetFilePathForKind(QString::fromUtf8(MIACODE_MOBILE_SFX_DIRECTORY), QString::fromLatin1(kind)));
        check(file.open(QIODevice::ReadOnly) && decodeMobileSfxWav(file.readAll(), &decoded, &error), kind);
    }
    TimelineNoteMarker a; a.type = "tap"; a.second = .5;
    auto b = a; b.lane = 2; b.isEx = true; b.isBreak = true;
    auto program = buildMobileSfxProgram({a, b}, 1, {}, true);
    auto count = [&](const char* kind) {
        return std::count_if(program.events.cbegin(), program.events.cend(), [&](const auto& event) { return event.kind == QLatin1String(kind); });
    };
    check(count("answer") == 1 && count("judge") == 1 && count("judge_break") == 1 && count("break") == 1 && count("ex") == 1,
        "same-time v2 aggregate, break and ex event semantics");

    MobileSfxMixer mixer; auto constant = ramp(); std::fill(constant.samples.begin(), constant.samples.end(), .2f);
    QVector<float> samples;
    auto tick = constant; tick.samples.resize(10);
    mixer.setClips({{"track_start", constant}, {"clock", tick}});
    mixer.setLevels(fullLevels());
    auto exportProgram = buildMobileExportAuditionProgram({}, 1, true, true, 4, 120, .5);
    mixer.setProgram(exportProgram); mixer.reset(-miacode::intro::kDurationSeconds, 1);
    samples = render(mixer, 10);
    check(near(samples[0], .1f) && near(samples[9], .1f), "intro opening uses its independent captured gain at negative head");
    mixer.reset(-2, 1);
    check(near(render(mixer, 10)[0], 0), "seeking into intro does not replay the opening sample");
    mixer.reset(0, 1); samples = render(mixer, 200);
    check(mixer.triggeredEvents() == 4 && near(samples[0], .2f) && near(samples[10], 0)
        && near(samples[50], .2f) && near(samples[100], .2f) && near(samples[150], .2f),
        "export clock PCM starts at chart zero and follows document beat spacing");
    mixer.reset(.75, 1); render(mixer, 100);
    check(mixer.triggeredEvents() == 2, "seeking past count-in skips elapsed clocks without replay");
    mixer.setProgram(buildMobileExportAuditionProgram({}, 1, true, false, 0, 120));
    mixer.reset(0, 1); samples = render(mixer, 200);
    check(mixer.triggeredEvents() == 0 && std::all_of(samples.cbegin(), samples.cend(), [](float x) { return x == 0; }),
        "leaving export removes intro and count-in PCM");
    mixer.setClips({{"judge", ramp()}, {"ex", constant}, {"touchhold", ramp()}, {"break_slide_tail_break", constant}});
    mixer.setLevels(fullLevels());
    MobileSfxProgram oneShots;
    oneShots.events = {{.1, "judge", 1}, {.15, "ex", 1}, {.2, "judge", 1}};
    mixer.setProgram(oneShots); mixer.reset(0, 1);
    samples = render(mixer, 30);
    check(near(samples[9], 0) && near(samples[14], .004f), "exact event boundary and native sample position");
    check(near(samples[16], .206f), "different note kinds mix together");
    check(near(samples[20], .2f) && near(samples[29], .209f), "latest same-kind event interrupts its older sample");
    mixer.reset(0, 2); samples = render(mixer, 15);
    check(near(samples[4], 0) && near(samples[5], 0) && near(samples[14], .204f), "2x schedules chart events while preserving SFX sample speed");
    auto muted = fullLevels(); muted.tapVolume = 0; muted.exVolume = 0; mixer.setLevels(muted);
    check(near(render(mixer, 1)[0], 0), "live levels mute existing voices");
    mixer.setLevels(fullLevels()); mixer.reset(.25, 1); samples = render(mixer, 10);
    check(std::all_of(samples.cbegin(), samples.cend(), [](float value) { return value == 0; }), "seek discards earlier one-shot voices");
    mixer.reset(0, 1); mixer.setEndSecond(.18); samples = render(mixer, 30);
    check(samples[17] > .2f && near(samples[18], 0) && near(samples[29], 0), "audio callback enforces range end even before UI pause");
    mixer.setEndSecond(std::numeric_limits<double>::infinity());

    MobileSfxProgram holds;
    holds.holds = miacode::preview_sfx_timeline::buildTouchholdOwnershipSegments({{0, 3}, {1, 2}});
    mixer.setProgram(holds); mixer.reset(0, 1); samples = render(mixer, 300);
    check(near(samples[50], .05f) && near(samples[150], .05f) && near(samples[250], .25f), "nested hold ownership restores the older source position");
    mixer.reset(2.4, 1); check(near(render(mixer, 1)[0], .24f), "seek into active hold restores its sample offset");
    mixer.reset(3, 1); check(near(render(mixer, 1)[0], 0), "hold ends exactly at its exclusive end");

    MobileSfxProgram tail; tail.events = {{0, "break_slide_tail_break", 1}};
    mixer.setProgram(tail); mixer.reset(0, 1); muted = fullLevels(); muted.breakSlideTailCheerMuted = true; mixer.setLevels(muted);
    check(near(render(mixer, 10)[0], 0), "tail cheer preference suppresses tail event");
    mixer.setLevels(fullLevels()); check(near(render(mixer, 1)[0], 0), "unmuting tail does not resurrect a suppressed old event");
    std::fprintf(stderr, "Mobile SFX mixer: %s; failures=%d\n", failures ? "failed" : "passed", failures);
    return failures ? 1 : 0;
}
