#include "runtime/Shared.h"

#include "app/services/ApplicationServices.h"

#include "audio/OfflineAudioDecoder.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "common/WaveformCache.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QStringList>
#include <QtMath>

namespace miacode::runtime::shared {

namespace {

const QList<double> kPreviewPlaybackRateOptions{
    0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0,
};

QString normalizeLanguageToken(QString token)
{
    token = token.trimmed().toLower();
    token.replace('-', '_');
    return token;
}

}  // namespace

int nearestPreviewPlaybackRateIndex(double rate)
{
    if (kPreviewPlaybackRateOptions.isEmpty()) {
        return -1;
    }
    int bestIndex = 0;
    double bestDiff = qAbs(kPreviewPlaybackRateOptions.first() - rate);
    for (int index = 1; index < kPreviewPlaybackRateOptions.size(); ++index) {
        const double diff = qAbs(kPreviewPlaybackRateOptions[index] - rate);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIndex = index;
        }
    }
    return bestIndex;
}

double steppedPreviewPlaybackRate(double rate, int direction)
{
    const int currentIndex = nearestPreviewPlaybackRateIndex(rate);
    if (currentIndex < 0) {
        return qMax(0.25, rate);
    }
    const int targetIndex = qBound(0, currentIndex + direction, kPreviewPlaybackRateOptions.size() - 1);
    return kPreviewPlaybackRateOptions[targetIndex];
}

SimaiNativeValidationLocale uiValidationLocale()
{
    // One implementation, owned by the non-Widget application layer. The name
    // remains as the shared runtime entry point for the parser/UI locale map.
    return miacode::uiValidationLocale();
}

QByteArray autosaveContentSignature(const QString& text)
{
    return QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
}

QString resolveProjectDataDirectoryPath(const QString& filePath)
{
    return miacode::waveform::projectDataDirectoryPathForFile(filePath);
}

void appendStartupTimingStage(const QString& stage, qint64 elapsedMs, qint64 deltaMs)
{
    miacode::debug_log::appendStartupTimingStage(stage, elapsedMs, deltaMs);
}

qint64 fileLastModifiedMs(const QFileInfo& fileInfo)
{
    return fileInfo.exists() ? fileInfo.lastModified().toMSecsSinceEpoch() : -1;
}

double probeAudioDurationSeconds(const QString& trackPath)
{
    return miacode::audio_decode::probeFileDurationSeconds(trackPath);
}

// See the declaration in Shared.h for why this must stay the only writer of
// `playing_`. Moved verbatim from ShellHost.cpp's `Session::setPreviewPlayingFlag`
// (Stage 4.9d-4b-2); the broadcast target changed from `Session::presentationChanged`
// (which only forwarded it, see SessionBootstrap.cpp) to `notifications` directly,
// the same retargeting PlaybackState.cpp's `updatePauseButtonAppearance` already did
// for the sibling presentation announcement. Stage 4.9e-4 retargeted the
// parameter from RuntimeContext::State to RuntimeContext::PlaybackState (see
// Shared.h) — kept the parameter NAME `state` so the `state_?\.playing_`
// single-writer grep/spec pattern still matches this exact line.
void writePreviewPlayingFlag(
    RuntimeContext::PlaybackState& state,
    miacode::ShellNotifications& notifications,
    bool playing)
{
    if (state.playing_ == playing) {
        return;
    }
    state.playing_ = playing;
    state.previewTransportState_ = playing
        ? miacode::PlaybackTransportState::Playing
        : (state.previewTransportState_ == miacode::PlaybackTransportState::Stopped
               ? miacode::PlaybackTransportState::Stopped
               : miacode::PlaybackTransportState::Paused);
    QMetaObject::invokeMethod(
        &notifications,
        [&notifications]() { emit notifications.presentationChanged(); },
        Qt::QueuedConnection);
}

}  // namespace miacode::runtime::shared
