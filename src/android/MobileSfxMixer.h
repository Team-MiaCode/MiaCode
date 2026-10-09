#pragma once
#include "audio/PreviewAudioSettings.h"
#include "common/PreviewSfxTimeline.h"
#include <QByteArray>
#include <QHash>
#include <QSpan>
#include <utility>
#include <limits>

namespace miacode::android {
struct MobileSfxClip {
    int sampleRate = 0;
    int channels = 0;
    QVector<float> samples;
    float sample(double second, int channel) const;
};
bool decodeMobileSfxWav(const QByteArray&, MobileSfxClip*, QString* error);
struct MobileSfxProgram {
    QVector<preview_sfx_timeline::ScheduledPlayback> events;
    QVector<preview_sfx_timeline::TouchholdOwnershipSegment> holds;
};
MobileSfxProgram buildMobileSfxProgram(const QVector<TimelineNoteMarker>&, double rate,
    const PreviewTimingSettings&, bool mineEnabled);
MobileSfxProgram buildMobileExportAuditionProgram(const QVector<TimelineNoteMarker>&, double rate,
    bool mineEnabled, bool introEnabled, int clockCount, double clockBpm, double introVolume = 1);

// Called on the audio callback, with ownership/synchronization supplied by output.
// Events and overlapping holds use the same v2 program as offline export.
class MobileSfxMixer {
public:
    void setClips(QHash<QString, MobileSfxClip> clips) { clips_ = std::move(clips); }
    void setIntroClip(MobileSfxClip clip) {
        voices_.clear();
        clips_.insert(QStringLiteral("track_start"), std::move(clip));
    }
    void setProgram(MobileSfxProgram program);
    void setLevels(PreviewAudioSettings levels);
    void reset(double second, double rate, bool includeCurrent = true);
    void setEndSecond(double second) { endSecond_ = qIsFinite(second) ? second : std::numeric_limits<double>::infinity(); }
    void render(QSpan<float> samples, int sampleRate, int channels);
    double nextSecond() const { return second_; }
    quint64 triggeredEvents() const { return triggered_; }
private:
    struct Voice { preview_sfx_timeline::ScheduledPlayback event; const MobileSfxClip* clip = nullptr; double gain = 0; };
    QHash<QString, MobileSfxClip> clips_;
    MobileSfxProgram program_;
    PreviewAudioSettings levels_;
    QVector<Voice> voices_;
    qsizetype nextEvent_ = 0;
    qsizetype hold_ = 0;
    double second_ = 0;
    double rate_ = 1;
    double endSecond_ = std::numeric_limits<double>::infinity();
    quint64 triggered_ = 0;
};
}
