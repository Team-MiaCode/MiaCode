#pragma once

#include <QVariantMap>

namespace miacode {

class MediaToolsEngine
{
public:
    virtual ~MediaToolsEngine() = default;

    // The two conversions run as jobs and report through JobProgressService.
    virtual void convertTrackTo44100Hz() = 0;
    virtual void compressBackgroundVideo() = 0;
    virtual void alignBackgroundVideoToTrack() = 0;

    // What the 前置空白 editor needs before it can offer anything: the current
    // media path, whether a backup exists, the detected bpm, and so on. An
    // empty map means the operation is not available for that media.
    virtual QVariantMap mediaBlankContext(bool isTrack) = 0;
    // Detected leading-silence timing for the same media, as a value map.
    virtual QVariantMap detectMediaBlankTiming(bool isTrack) = 0;

    virtual void restoreMediaBlankBackup(bool isTrack) = 0;
    virtual void applyMediaBlank(bool isTrack, double beats, double bpm) = 0;

protected:
    MediaToolsEngine() = default;
    MediaToolsEngine(const MediaToolsEngine&) = default;
    MediaToolsEngine& operator=(const MediaToolsEngine&) = default;
};

}  // namespace miacode
