#include "MobileSfxMixer.h"
#include "common/IntroConfig.h"
#include <QtEndian>
#include <bit>
#include <cmath>

namespace miacode::android {
float MobileSfxClip::sample(double second, int channel) const {
    if (!qIsFinite(second) || second < 0 || channels < 1 || samples.isEmpty()) return 0;
    const double position = second * sampleRate;
    if (position >= samples.size() / channels) return 0;
    const qsizetype frame = static_cast<qsizetype>(position);
    const int sourceChannel = channels == 1 ? 0 : qMin(channel, channels - 1);
    const float a = samples[frame * channels + sourceChannel];
    const float b = frame + 1 < samples.size() / channels ? samples[(frame + 1) * channels + sourceChannel] : 0;
    return a + (b - a) * float(position - frame);
}

bool decodeMobileSfxWav(const QByteArray& data, MobileSfxClip* clip, QString* error) {
    const auto reject = [&](const char* text) { if (error) *error = QString::fromLatin1(text); return false; };
    if (!clip || data.size() < 12 || data.size() > 64 * 1024 * 1024
        || data.first(4) != "RIFF" || data.sliced(8, 4) != "WAVE") return reject("Invalid or oversized WAV");
    const auto u16 = [&](qsizetype at) { return qFromLittleEndian<quint16>(data.constData() + at); };
    const auto u32 = [&](qsizetype at) { return qFromLittleEndian<quint32>(data.constData() + at); };
    int format = 0, channels = 0, sampleRate = 0, alignment = 0, bits = 0;
    QByteArray pcm;
    for (qsizetype at = 12; at + 8 <= data.size();) {
        const quint32 length = u32(at + 4);
        const qsizetype begin = at + 8;
        if (length > quint64(data.size() - begin)) return reject("Truncated WAV chunk");
        const auto kind = data.sliced(at, 4);
        if (kind == "fmt ") {
            if (length < 16) return reject("Truncated WAV format");
            format = u16(begin); channels = u16(begin + 2); sampleRate = int(u32(begin + 4));
            alignment = u16(begin + 12); bits = u16(begin + 14);
            if (format == 0xfffe && length >= 40) format = u16(begin + 24);
        } else if (kind == "data") pcm.append(data.constData() + begin, length);
        at = begin + length + (length & 1);
    }
    if (channels < 1 || channels > 2 || sampleRate < 8000 || sampleRate > 192000
        || alignment != channels * (bits / 8) || pcm.isEmpty() || alignment < 1 || pcm.size() % alignment
        || !((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
        return reject("Unsupported or inconsistent WAV format");
    MobileSfxClip decoded; decoded.channels = channels; decoded.sampleRate = sampleRate;
    decoded.samples.resize(pcm.size() / (bits / 8));
    for (qsizetype i = 0; i < decoded.samples.size(); ++i) {
        const auto* source = reinterpret_cast<const uchar*>(pcm.constData() + i * (bits / 8));
        float value = 0;
        if (format == 3) value = std::bit_cast<float>(qFromLittleEndian<quint32>(source));
        else if (bits == 8) value = (int(source[0]) - 128) / 128.0f;
        else if (bits == 16) value = qFromLittleEndian<qint16>(source) / 32768.0f;
        else if (bits == 24) {
            const qint32 raw = source[0] | (source[1] << 8) | (source[2] << 16);
            value = ((raw & 0x800000) ? raw - 0x1000000 : raw) / 8388608.0f;
        } else value = qFromLittleEndian<qint32>(source) / 2147483648.0f;
        if (!std::isfinite(value)) return reject("Non-finite WAV sample");
        decoded.samples[i] = value;
    }
    *clip = std::move(decoded); if (error) error->clear(); return true;
}

MobileSfxProgram buildMobileSfxProgram(const QVector<TimelineNoteMarker>& markers, double rate,
    const PreviewTimingSettings& timing, bool mineEnabled) {
    QVector<preview_sfx_timeline::Event> events;
    QVector<preview_sfx_timeline::TouchholdSpan> spans;
    preview_sfx_timeline::buildTimeline(markers, rate, timing, &events, &spans, mineEnabled);
    return {preview_sfx_timeline::buildScheduledPlaybacks(events), preview_sfx_timeline::buildTouchholdOwnershipSegments(spans)};
}

MobileSfxProgram buildMobileExportAuditionProgram(const QVector<TimelineNoteMarker>& markers, double rate,
    bool mineEnabled, bool introEnabled, int clockCount, double clockBpm, double introVolume) {
    auto program = buildMobileSfxProgram(markers, rate, {}, mineEnabled);
    if (introEnabled) {
        // Chart audio starts at zero after the negative-time intro hands off.
        program.events.removeIf([](const auto& event) { return event.second < 0; });
        program.holds.removeIf([](const auto& hold) { return hold.endSecond <= 0; });
        for (auto& hold : program.holds) {
            if (hold.startSecond < 0) {
                hold.sourceOffsetSecond += -hold.startSecond / preview_sfx_timing::normalizedPlaybackRate(rate);
                hold.startSecond = 0;
            }
        }
        program.events.append({-intro::kDurationSeconds, QStringLiteral("track_start"),
            qIsFinite(introVolume) ? qBound(0.0, introVolume, 2.0) : 1});
    }
    if (clockCount > 0 && qIsFinite(clockBpm) && clockBpm > 0) {
        const double beat = 60.0 / clockBpm;
        if (qIsFinite(beat) && beat > 0)
            for (int index = 0; index < clockCount; ++index)
                program.events.append({index * beat, QStringLiteral("clock"), 1});
    }
    std::stable_sort(program.events.begin(), program.events.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });
    return program;
}

void MobileSfxMixer::setProgram(MobileSfxProgram program) {
    program_ = std::move(program);
    voices_.clear(); voices_.reserve(qMin<qsizetype>(program_.events.size(), 64));
    reset(second_, rate_, false);
}
void MobileSfxMixer::setLevels(PreviewAudioSettings levels) {
    levels.normalize(); levels_ = levels;
    if (levels_.breakSlideTailCheerMuted)
        voices_.removeIf([](const Voice& voice) { return voice.event.kind == "break_slide_tail_break"; });
}
void MobileSfxMixer::reset(double second, double rate, bool includeCurrent) {
    second_ = qIsFinite(second) ? second : 0;
    rate_ = preview_sfx_timing::normalizedPlaybackRate(rate);
    voices_.clear(); triggered_ = 0;
    nextEvent_ = std::lower_bound(program_.events.cbegin(), program_.events.cend(), second_,
        [includeCurrent](const auto& event, double value) {
            return includeCurrent ? event.second + 1e-9 < value : event.second <= value + 1e-9;
        }) - program_.events.cbegin();
    hold_ = 0;
    while (hold_ < program_.holds.size() && program_.holds[hold_].endSecond <= second_) ++hold_;
}

void MobileSfxMixer::render(QSpan<float> samples, int sampleRate, int channels) {
    std::fill(samples.begin(), samples.end(), 0);
    if (sampleRate <= 0 || channels < 1 || samples.size() % channels) return;
    const double step = rate_ / sampleRate;
    const auto holdClip = clips_.constFind("touchhold");
    const double holdGain = previewSfxVolumeForKind(levels_, "touchhold");
    const auto gain = [&](const auto& event) {
        // Capture the separate intro volume in the program on the GUI thread;
        // the audio callback never reads its process-global mutable preference.
        if (event.kind == "track_start") return event.gain;
        return levels_.breakSlideTailCheerMuted && event.kind == "break_slide_tail_break"
            ? 0.0 : event.gain * previewSfxVolumeForKind(levels_, event.kind);
    };
    for (auto& voice : voices_) voice.gain = gain(voice.event);
    for (qsizetype frame = 0; frame < samples.size() / channels; ++frame) {
        if (second_ >= endSecond_ - 1e-9) { second_ += step; continue; }
        while (nextEvent_ < program_.events.size() && program_.events[nextEvent_].second <= second_ + 1e-9) {
            const auto& event = program_.events[nextEvent_++];
            if (levels_.breakSlideTailCheerMuted && event.kind == "break_slide_tail_break") continue;
            const auto clip = clips_.constFind(event.kind);
            if (clip == clips_.cend()) continue;
            if (previewSfxShouldInterruptPreviousKind(event.kind))
                voices_.removeIf([&](const Voice& voice) { return voice.event.kind == event.kind; });
            voices_.append({event, &clip.value(), gain(event)}); ++triggered_;
        }
        voices_.removeIf([&](const Voice& voice) {
            return (second_ - voice.event.second) / rate_ >= double(voice.clip->samples.size() / voice.clip->channels) / voice.clip->sampleRate;
        });
        while (hold_ < program_.holds.size() && program_.holds[hold_].endSecond <= second_ + 1e-9) ++hold_;
        const auto* hold = hold_ < program_.holds.size() && program_.holds[hold_].startSecond <= second_ + 1e-9
            ? &program_.holds[hold_] : nullptr;
        for (int channel = 0; channel < channels; ++channel) {
            double mixed = 0;
            for (const auto& voice : voices_) {
                mixed += voice.clip->sample((second_ - voice.event.second) / rate_, channel) * voice.gain;
            }
            if (hold && holdClip != clips_.cend())
                mixed += holdClip->sample(hold->sourceOffsetSecond + (second_ - hold->startSecond) / rate_, channel)
                    * holdGain;
            samples[frame * channels + channel] = float(qBound(-1.0, mixed, 1.0));
        }
        second_ += step;
    }
}
}
