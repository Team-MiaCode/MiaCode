#include "app/runtime/playback/PlaybackCoordinator.h"
#include "app/runtime/Session.h"
#include "app/runtime/Shared.h"
#include "app/runtime/shell/ShellHost.h"

#include "audio/QtPreviewSfxRuntime.h"
#include "core/chart/parser/SimaiParser.h"
#include "app/quick_shell/QuickShellPreviewCompositeSurface.h"
#include "app/quick_shell/QuickShellPreviewSurfacePolicy.h"
#include "core/chart/ChartAssetPaths.h"
#include "app/runtime/ContentDurationConfig.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "core/video/PreviewInteractionConfig.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/stage_media/PreviewStageMediaHost.h"
#include "core/scene/PreviewProgressStatsCache.h"
#include "core/chart/transform/ChartBatchTransform.h"
#include "core/chart/transform/ChartNormalization.h"
#include "timeline/quick/TimelineQuickStateBridge.h"
#include "core/analysis/MuriAnalyzer.h"
#include "core/analysis/MuriPanelEntries.h"
#include "core/analysis/MuriStaticChecker.h"

#include <QtCore>
#include <QtGui>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

using namespace miacode::runtime::shared;
