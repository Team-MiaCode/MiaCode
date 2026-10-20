#pragma once

#include <QByteArray>
#include <QFont>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "core/chart/parser/SimaiParser.h"
#include "app/runtime/shell/WindowParityMetrics.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "app/runtime/RuntimeContext.h"

class QFileInfo;
class QObject;

namespace miacode {
class ShellNotifications;
}

namespace miacode::runtime::shared {

// Single writer for the preview pause second.
//
// Why this exists: `pauseSecond_` was written directly from many call sites,
// none of them logged, and one of them (`applyQtPreviewPosition`) fires unconditionally
// on every playback tick. A reported symptom — the timeline stepping BACKWARD some time
// after a pause, by roughly the length of an audio-device stall — was therefore
// un-diagnosable: nothing recorded who moved the playhead, when, or from where, so every
// explanation for it was a guess from reading code, and two such guesses were wrong.
//
// Routing a write through here makes a backward move name its own author. Only backward
// moves while paused are logged: forward motion during playback is the normal per-frame
// case and would bury the channel.
//
// The invariant this depends on is a rule, not a headcount: `pauseSecond_` must
// never be assigned except through this function. Check it with
//
//   grep -rn "pauseSecond_[[:space:]]*=" src --include="*.cpp" --include="*.h" \
//     --include="*.inc" | grep -v writePreviewPauseSecond
//
// which should match only the declaration and the reference alias in
// SessionMembers.inc. Any other hit is a hole: that writer can move the pause
// second backward without leaving a row, which defeats the whole point of the channel.
// One such hole shipped and was missed by the original "every write is routed" claim —
// `latency::LatencySandboxController::applyPlayheadToScene`, outside the Session
// section files the audit looked at — so run the grep rather than trusting this comment.
inline void writePreviewPauseSecond(
    double& slot,
    double next,
    bool previewPlaying,
    const char* reason)
{
    const double previous = slot;
    slot = next;
    if (previewPlaying || !miacode::debug_options::runtimeDebugOutputEnabled()) {
        return;
    }
    // Half a timeline frame at 120 Hz — below this a "move" is re-flush noise from the
    // same logical position, not a jump anyone can see.
    constexpr double kBackwardMoveEpsilonSeconds = 0.004;
    if (!(next < previous - kBackwardMoveEpsilonSeconds)) {
        return;
    }
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Audio,
        QStringLiteral("preview/playback"),
        QStringLiteral("action=pause_second_moved_back reason=%1 from=%2 to=%3 delta_ms=%4")
            .arg(QLatin1String(reason))
            .arg(previous, 0, 'f', 6)
            .arg(next, 0, 'f', 6)
            .arg((next - previous) * 1000.0, 0, 'f', 3));
}

// Publish playback state changes through the shared notification path.
void writePreviewPlayingFlag(
    RuntimeContext::PlaybackState& state,
    miacode::ShellNotifications& notifications,
    bool playing);

inline constexpr int kEmbeddedPreviewPanelMinWidth = miacode::window_parity::kEmbeddedPreviewPanelMinWidth;
inline constexpr int kPreviewPanelMarginX = miacode::window_parity::kPreviewPanelMarginX;
inline constexpr int kPreviewControlStatsCardMinWidth = miacode::window_parity::kPreviewControlStatsCardMinWidth;
inline constexpr int kEditorTextFontSizeMin = 8;
inline constexpr int kEditorTextFontSizeMax = 28;
inline constexpr double kEditorLineSpacingFactorDefault = 1.0;
inline constexpr int kAutosaveIntervalMs = 2 * 60 * 1000;
inline constexpr int kAutosaveHistoryMaxVersions = 30;
inline constexpr int kAutosaveLatestIdleMs = 2 * 1000;
inline constexpr double kTimelineMaxUiUpdateFps = 3600.0;
// Cap interactive preview scrub updates at <= ? FPS so timeline dragging and
// preview-slider dragging do not spam seek work faster than the video path can settle.
inline constexpr int kPreviewScrubRenderIntervalMs = 67;

extern const QList<double> kEditorLineSpacingFactorOptions;

double normalizeEditorLineSpacingFactor(double factor);
QString editorLineSpacingFactorLabel(double factor);
int nearestPreviewPlaybackRateIndex(double rate);
double steppedPreviewPlaybackRate(double rate, int direction);
// The parser validation locale matching the session UI language.
SimaiValidationLocale uiValidationLocale();
QByteArray autosaveContentSignature(const QString& text);
QString resolveProjectDataDirectoryPath(const QString& filePath);
void appendStartupTimingStage(const QString& stage, qint64 elapsedMs, qint64 deltaMs);
QFont editorFont(int pointSize = -1);
int blockSpacingPixelsForPointSize(int pointSize, double spacingFactor);
qint64 fileLastModifiedMs(const QFileInfo& fileInfo);
double probeAudioDurationSeconds(const QString& trackPath);

void ensurePreviewSfxRuntimePrepared(RuntimeContext::State& state);
bool previewStageMediaRouteHasVideo(const RuntimeContext::State& state);
void pausePreviewStageMediaRoutePlayback(RuntimeContext::State& state);
void syncPreviewStageMediaRoutePlayback(RuntimeContext::State& state, double second);
void setPreviewStageMediaRouteObservedPlayheadSecond(RuntimeContext::State& state, double second);
void resetPreviewStageMediaRouteTimelineOffset(RuntimeContext::State& state);
void submitPreviewStageMediaRoutePausedSeek(RuntimeContext::State& state, double second, quint64 generation);
void seekPreviewStageMediaRouteWhilePaused(RuntimeContext::State& state, double second);
void applyPreviewStageMediaRouteVisualSettings(RuntimeContext::State& state);
void applyPreviewStageMediaRoutePlaybackRate(RuntimeContext::State& state, double rate, const char* site = nullptr);
// owner is only used as the QuickShellPreviewCompositeSurface's QObject
// parent, never as a Session — see Shared.Preview.cpp.
void refreshQuickShellPreviewCompositeSurfaceState(RuntimeContext::State& state, QObject& owner);
void refreshPreviewStageMediaRouteDebugState(RuntimeContext::State& state, bool requestUpdate = true);
// Windows-only body (Q_OS_WIN); a no-op elsewhere. See the definition for why the
// platform conditional must not be simplified away.
void setPreviewFixedTimerHighResolutionActive(RuntimeContext::State& state, bool active);
// Skin-directory / outline-directory resolution: pure filesystem + asset-path
// lookups, no member state. previewSkinDisplayName and availablePreviewSkinDirectoryNames
// depend on the small name-normalization helpers below, which used to be
// WarmupAndSettings.cpp anonymous-namespace-local; promoted here so both
// StageMediaHost and PlaybackCoordinator can call the same implementation.
QString standardPreviewSkinDirectoryName();
QString dxPreviewSkinDirectoryName();
QString normalizePreviewSkinDirectoryName(QString name);
bool hasCorePreviewSkinAssets(const QString& directory);
QString resolvePreviewSkinRootDir();
QString resolvePreviewCustomOutlineDir();
QStringList availablePreviewSkinDirectoryNames();
QString previewSkinDisplayName(const QString& directoryName);
QStringList availablePreviewCustomOutlineFileNames();
QString previewCustomOutlineDisplayName(const QString& fileName);

}  // namespace miacode::runtime::shared
