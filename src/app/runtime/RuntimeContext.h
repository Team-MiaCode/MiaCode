#pragma once

#include <functional>
#include <memory>

#include <QtCore>
#include <QtGui>

#include "audio/PreviewAudioSettings.h"
#include "core/video/PreviewRenderSettings.h"
#include "core/chart/document/SimaiDocument.h"
#include "core/chart/parser/SimaiParser.h"
#include "core/chart/document/SimaiTimingMetadata.h"
#include "core/analysis/MuriRenderOptions.h"
#include "core/analysis/MuriTypes.h"
#include "core/video/PreviewGameplayConfig.h"
#include "core/video/PreviewTimingSettings.h"
#include "core/video/PreviewVideoGeometryConfig.h"
#include "core/chart/transform/ChartNormalization.h"
#include "app/services/PlaybackControl.h"
#include "core/chart/model/TimelineData.h"
#include "timeline/TimelineQuickModel.h"
#include "core/analysis/TimelineSlowRefresh.h"
#include "export/video_export/VideoExportSnapshot.h"

class IntroBannerSpec;
class PreviewAudioDeviceWatcher;
class PreviewRuntime;
class PreviewStageMediaHost;
namespace miacode {
class ExportPagePort;
}
class QtPreviewSfxRuntime;
class QuickShellPreviewCompositeSurface;
class TimelineQuickStateBridge;

namespace miacode::preview::scene {
class PreviewProgressStatsCache;
}

namespace miacode::waveform {
class WaveformCacheService;
struct WaveformData;
}

namespace miacode {
class JobProgressService;
class UiRequestService;
}

namespace miacode::runtime {

// Transitional runtime storage boundary. The two nested records are kept
// together for now because the legacy runtime still has cross-domain reads,
// but they no longer form part of Session's type definition. This lets each
// host declare the storage boundary it actually borrows and keeps playback
// contract headers independent from Session internals.
class RuntimeContext final
{
public:
    enum class TextEncoding {
        Utf8,
        System,
    };

    enum class BottomTabsTabId {
        Timeline,
        Validation,
        Muri,
        Unknown,
    };

    struct ValidationCachedIssue {
        int line = 1;
        int col = 1;
        int endCol = 1;
        SimaiValidationSeverity severity = SimaiValidationSeverity::Error;
        QString rawMessage;
        QString displayMessage;
    };

    struct ValidationDecoration {
        int line = 1;
        int col = 1;
        int endCol = 1;
        QString message;
        bool warning = false;
    };

    struct ValidationCacheEntry {
        QString chartText;
        SimaiValidationLocale validationLocale = SimaiValidationLocale::English;
        miacode::simai::SimaiTimingMetadata timingMetadata;
        quint64 validationRevision = 0;
        bool ok = true;
        int errorCount = 0;
        int warningCount = 0;
        int strictNoteCount = 0;
        int strictErrorCount = 0;
        QVector<ValidationCachedIssue> issues;
    };

    struct DeletedDifficultyUndoState {
        bool valid = false;
        bool wasActive = false;
        int difficultyId = 0;
        SimaiDifficultyData difficultyData;
    };


    struct TimelineState {
        TimelineQuickStateBridge* timelineQuickStateBridge_ = nullptr;
        bool timelineReady_ = false;
        quint64 deferredTimelineBridgeFlushGeneration_ = 0;
        bool pendingQuickTimelineCursorSync_ = false;
        double pendingQuickTimelineCursorSecond_ = 0.0;
        bool pendingQuickTimelineCursorCenterView_ = false;
        int lastTimelineParseDifficultyId_ = 0;
        QString lastTimelineParseChartText_;
        miacode::simai::SimaiTimingMetadata lastTimelineParseTimingMetadata_;
        SimaiParseResult lastTimelineParseResult_;
        QVector<TimelineNoteMarker> latestTimelineNoteMarkers_;
        QByteArray latestTimelineNoteMarkerSignature_;
        quint64 latestTimelinePreviewRevision_ = 0;
        bool latestTimelinePreviewSnapshotReady_ = false;
        TimelineQuickModel timelineQuickModel_;
        TimelineSlowRefreshRequest pendingTimelineSlowRefresh_;
        TimelineSlowRefreshRequest pendingTimelineAnalysisRefresh_;
        quint64 timelineRevision_ = 0;
        quint64 timelineSlowRequestedRevision_ = 0;
        quint64 timelineAnalysisRequestedRevision_ = 0;
        quint64 waveformRefreshGeneration_ = 0;
        PreviewCanvasFrameRateMode timelineFrameRateMode_ = PreviewCanvasFrameRateMode::DisplayRefresh;
        bool timelineSyncEnabled_ = false;
    };

    struct PlaybackState {
        bool playing_ = false;
        miacode::PlaybackTransportState previewTransportState_ =
            miacode::PlaybackTransportState::Stopped;
        double pauseSecond_ = 0.0;
        double previewPlaybackRate_ = 1.0;
        double qtPreviewStartSecond_ = 0.0;
        QElapsedTimer qtPreviewElapsed_;
        // Startup handshake fields consulted directly by
        // authoritativeAudioClockSecond() to decide the frozen "now" second
        // while audio/video prepare asynchronously. The rest of the startup
        // handshake cluster (previewStartupAudioPrepared_,
        // previewStartupCanvasPresented_, ...) stays in State: those orchestrate
        // the handshake's progress rather than answering "where is the
        // playhead now".
        bool previewStartupSyncPending_ = false;
        bool previewLateVideoStartPending_ = false;
        double previewStartupPreparedSecond_ = 0.0;
    };

#define MIACODE_RUNTIME_CONTEXT_TYPES 1
#include "app/runtime/SessionMembers.inc"
#undef MIACODE_RUNTIME_CONTEXT_TYPES

    // Declaration order is load-bearing: `state` binds compatibility references
    // into `timeline` and `playback`, so both records must be declared (and
    // therefore constructed) first. Copying is deleted because a copied
    // `state` would still alias the source context's timeline/playback
    // storage.
    RuntimeContext()
        : state(timeline, playback)
    {}

    RuntimeContext(const RuntimeContext&) = delete;
    RuntimeContext& operator=(const RuntimeContext&) = delete;

    TimelineState timeline;
    PlaybackState playback;
    Ui ui;
    State state;
};

}  // namespace miacode::runtime
