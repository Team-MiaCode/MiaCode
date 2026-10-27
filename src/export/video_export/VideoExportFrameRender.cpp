#include "export/video_export/VideoExportController.h"

#include "export/video_export/BassExportAudioBackend.h"
#include "export/video_export/RawVideoPipeTransport.h"
#include "export/video_export/VideoExportAudioRenderPlan.h"
#include "export/video_export/VideoExportQuickRenderBackend.h"
#include "export/video_export/VideoExportRuntimePolicy.h"
#include "core/video/AssetPaths.h"
#include "core/chart/ChartAssetPaths.h"
#include "core/chart/IntroConfig.h"
#include "common/DebugLog.h"
#include "common/OperationLog.h"
#include "common/DebugOptions.h"
#include "core/video/LayoutRingConfig.h"
#include "audio/PreviewAudioMixConfig.h"
#include "core/video/PreviewGameplayConfig.h"
#include "core/scene/PreviewSceneGeometry.h"
#include "core/scene/PreviewSfxTimeline.h"
#include "preview/runtime/PreviewSceneAssetLoader.h"
#include "core/analysis/MuriAnalyzer.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QRect>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUuid>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "export/video_export/VideoExportControllerInternal.h"

// VideoExportFrameRender.cpp — lead-in pause overlay, ready-frame payload assembly, GPU/offscreen frame rendering + PBO drain, and packed-RGBA frame preparation.
//
// Definitions extracted verbatim from the original VideoExportController.cpp
// during the god-file split. All helpers live in the shared
// miacode::video_export::detail namespace (declared in
// VideoExportControllerInternal.h).
using namespace miacode::video_export::detail;

namespace miacode::video_export::detail {

// Paint a semi-transparent two-bar pause glyph centred over the export
// frame. Called once per frame for the lead-in / pre-range window so the
// viewer immediately sees that the playfield is held in a stationary state
// (chart frozen at segmentStart, no playback yet). The glyph disappears
// the moment the lead-in ends and the chart starts to advance.
//
// Every dimension derives from the shared PauseGlyphGeometry, which scales
// from the frame's short side — one knob resizes the whole badge.
//
// Opaque export frames retain their readback format through packing.
namespace {

struct PauseGlyphGeometry {
    QPointF center;
    qreal diameter = 0.0;
    qreal radius = 0.0;
    QRectF badge;
    QRectF leftBar;
    QRectF rightBar;
};

PauseGlyphGeometry pauseGlyphGeometry(const QSize& frameSize)
{
    PauseGlyphGeometry geometry;
    const int shortSide = qMin(frameSize.width(), frameSize.height());
    geometry.center = QPointF(frameSize.width() / 2.0, frameSize.height() / 2.0);
    geometry.diameter = qMax<qreal>(28.0, shortSide * 0.17);
    geometry.radius = geometry.diameter / 2.0;
    geometry.badge = QRectF(
        geometry.center.x() - geometry.radius,
        geometry.center.y() - geometry.radius,
        geometry.diameter,
        geometry.diameter
    );
    // 0.40 x 0.115 of the diameter reads as the standard pause mark, with a
    // comfortable margin left inside the disc.
    const qreal barHeight = geometry.diameter * 0.40;
    const qreal barWidth = geometry.diameter * 0.115;
    const qreal gap = barWidth * 0.85;
    const qreal totalWidth = barWidth * 2.0 + gap;
    const qreal x0 = geometry.center.x() - totalWidth / 2.0;
    const qreal y0 = geometry.center.y() - barHeight / 2.0;
    geometry.leftBar = QRectF(x0, y0, barWidth, barHeight);
    geometry.rightBar = QRectF(x0 + barWidth + gap, y0, barWidth, barHeight);
    return geometry;
}

// Full-frame dim. SourceOver onto an opaque chart frame pulls its perceived
// brightness down; onto a transparent base it leaves a dark wash that FFmpeg's
// overlay filter then composites over the chart background. Either way the
// playfield reads paused.
void drawPauseFrameDim(QPainter& painter, const QRect& frameRect, int alpha)
{
    painter.fillRect(frameRect, QColor(0, 0, 0, alpha));
}

void drawPauseBars(
    QPainter& painter,
    const PauseGlyphGeometry& geometry,
    const QColor& color,
    qreal cornerFactor
)
{
    const qreal radius = geometry.leftBar.width() * cornerFactor;
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(geometry.leftBar, radius, radius);
    painter.drawRoundedRect(geometry.rightBar, radius, radius);
}

void strokePauseRing(
    QPainter& painter,
    const QRectF& badge,
    const QColor& color,
    qreal width
)
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, width));
    painter.drawEllipse(badge.adjusted(width / 2.0, width / 2.0, -width / 2.0, -width / 2.0));
    painter.setPen(Qt::NoPen);
}

void drawPauseGlyphDarkRing(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);
    painter.setBrush(QColor(17, 19, 26, 214));
    painter.drawEllipse(g.badge);
    strokePauseRing(painter, g.badge, QColor(255, 255, 255, 200), qMax<qreal>(1.5, g.diameter * 0.028));
    drawPauseBars(painter, g, QColor(255, 255, 255, 255), 0.0);
}

}  // namespace

void drawLeadInPauseOverlay(QImage* frame)
{
    if (frame == nullptr || frame->isNull()) {
        return;
    }
    const QSize frameSize = frame->size();
    if (qMin(frameSize.width(), frameSize.height()) <= 0) {
        return;
    }

    QPainter painter(frame);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    const PauseGlyphGeometry geometry = pauseGlyphGeometry(frameSize);
    drawPauseGlyphDarkRing(painter, frame->rect(), geometry);
}

ReadyFramePayload buildReadyFramePayload(
    VideoExportQuickRenderBackend* exportBackend,
    int frameIndex,
    double exportSecond,
    QVector<ObjectTraceItem>&& traceItems,
    QImage frame,
    qint64 renderNs,
    bool usedOffscreenPath
)
{
    ReadyFramePayload readyFrame;
    readyFrame.frameIndex = frameIndex;
    readyFrame.exportSecond = exportSecond;
    readyFrame.traceItems = std::move(traceItems);
    readyFrame.frame = std::move(frame);
    readyFrame.renderNs = renderNs;
    readyFrame.usedOffscreenPath = usedOffscreenPath;
    if (exportBackend != nullptr) {
        readyFrame.offscreenDrawNs = exportBackend->offscreenDrawNsLastFrameForDebug();
        readyFrame.offscreenReadbackNs = exportBackend->offscreenReadbackNsLastFrameForDebug();
        readyFrame.stateUpdateNs = exportBackend->stateUpdateNsLastFrameForDebug();
        readyFrame.polishNs = exportBackend->polishNsLastFrameForDebug();
        readyFrame.syncNs = exportBackend->syncNsLastFrameForDebug();
        readyFrame.renderSubmitNs = exportBackend->renderSubmitNsLastFrameForDebug();
        readyFrame.fallbackCount = exportBackend->cpuFallbackCountLastFrameForDebug();
        readyFrame.usedGpuRenderer = exportBackend->usedGpuRendererLastFrameForDebug();
    }
    return readyFrame;
}

QImage renderExportFrameSynchronously(
    VideoExportQuickRenderBackend* exportBackend,
    bool useOffscreenGpu,
    const QSize& frameSize,
    const ExportFrameRenderParams& params
)
{
    return useOffscreenGpu
        ? exportBackend->renderFrameOffscreen(
              frameSize,
              params.exportSecond,
              params.showTimestamp,
              params.showObjectStatsHud,
              params.hudPlayheadSecondsOverride)
        : exportBackend->renderFrame(
              frameSize,
              params.exportSecond,
              params.showTimestamp,
              params.showObjectStatsHud,
              params.hudPlayheadSecondsOverride);
}

ExportFrameRenderStatus renderExportFrameWithConfiguredBackend(
    VideoExportQuickRenderBackend* exportBackend,
    bool* useOffscreenGpu,
    bool* useOffscreenPboReadback,
    std::deque<PendingPboFrame>* pendingPboFrames,
    const QSize& frameSize,
    const ExportFrameRenderParams& params,
    QVector<ObjectTraceItem>&& traceItems,
    std::vector<ReadyFramePayload>* readyFrames,
    QString* fallbackDetail
)
{
    if (exportBackend == nullptr
        || useOffscreenGpu == nullptr
        || useOffscreenPboReadback == nullptr
        || pendingPboFrames == nullptr
        || readyFrames == nullptr) {
        return ExportFrameRenderStatus::Failed;
    }

    readyFrames->clear();
    QElapsedTimer frameTimer;
    frameTimer.start();
    bool usedOffscreenPath = false;
    QImage frame;

    if (*useOffscreenGpu && *useOffscreenPboReadback) {
        QImage completedFrame;
        bool completedFrameReady = false;
        QString pboStepError;
        const bool pboStepOk = exportBackend->renderFrameOffscreenPboStep(
            frameSize,
            params.exportSecond,
            params.showTimestamp,
            params.showObjectStatsHud,
            &completedFrame,
            &completedFrameReady,
            false,
            &pboStepError,
            params.hudPlayheadSecondsOverride
        );
        const qint64 renderNs = frameTimer.nsecsElapsed();
        if (pboStepOk) {
            usedOffscreenPath = true;
            // Two pending slots in steady state since the convert worker
            // adds an extra frame of pipeline depth on top of the PBO
            // ping-pong: oldest entry = frame whose worker job just
            // finished and is being returned to us in completedFrame;
            // newer entry = frame currently in a PBO awaiting either
            // worker submission or readback finalisation.
            const bool producedReadyFrame = completedFrameReady && !pendingPboFrames->empty();
            if (producedReadyFrame) {
                PendingPboFrame oldest = std::move(pendingPboFrames->front());
                pendingPboFrames->pop_front();
                readyFrames->push_back(buildReadyFramePayload(
                    exportBackend,
                    oldest.params.frameIndex,
                    oldest.params.exportSecond,
                    std::move(oldest.traceItems),
                    std::move(completedFrame),
                    renderNs,
                    true
                ));
            }
            PendingPboFrame newPending;
            newPending.valid = true;
            newPending.params = params;
            newPending.traceItems = std::move(traceItems);
            pendingPboFrames->push_back(std::move(newPending));
            return producedReadyFrame ? ExportFrameRenderStatus::Ready : ExportFrameRenderStatus::Deferred;
        }

        appendRenderBackendFallbackDetail(
            fallbackDetail,
            QStringLiteral("frame=%1 reason=offscreen_pbo_failed error=%2")
                .arg(params.frameIndex)
                .arg(pboStepError)
        );
        exportBackend->resetOffscreenPboReadback();
        *useOffscreenPboReadback = false;
        // The reset abandons the GPU side of the pipeline, so the frames it
        // still owed us can only be produced by drawing them again here,
        // before the frame we were actually called for. Dropping them would
        // renumber the rest of the export instead of leaving a visible gap
        // (see redrawPendingPipelineFrames).
        const bool useOffscreenForRedraw = *useOffscreenGpu;
        const bool redrawOk = redrawPendingPipelineFrames(
            pendingPboFrames,
            [&](PendingPboFrame& pending, ReadyFramePayload* payload) {
                QElapsedTimer redrawTimer;
                redrawTimer.start();
                if (pending.params.introApplied) {
                    exportBackend->setIntroFrame(
                        pending.params.introAuthoringFrame, pending.params.introActive, pending.params.outputSecond);
                }
                QImage redrawnFrame = renderExportFrameSynchronously(
                    exportBackend, useOffscreenForRedraw, frameSize, pending.params);
                if (redrawnFrame.isNull()) {
                    appendRenderBackendFallbackDetail(
                        fallbackDetail,
                        QStringLiteral("frame=%1 reason=pending_frame_redraw_failed")
                            .arg(pending.params.frameIndex)
                    );
                    return false;
                }
                *payload = buildReadyFramePayload(
                    exportBackend,
                    pending.params.frameIndex,
                    pending.params.exportSecond,
                    std::move(pending.traceItems),
                    std::move(redrawnFrame),
                    redrawTimer.nsecsElapsed(),
                    useOffscreenForRedraw
                );
                appendRenderBackendFallbackDetail(
                    fallbackDetail,
                    QStringLiteral("frame=%1 reason=pending_frame_redrawn")
                        .arg(pending.params.frameIndex)
                );
                return true;
            },
            readyFrames
        );
        if (!redrawOk) {
            return ExportFrameRenderStatus::Failed;
        }
        // The redraws rewound the scene's intro state to older frames; put it
        // back where the current frame expects it.
        if (params.introApplied) {
            exportBackend->setIntroFrame(params.introAuthoringFrame, params.introActive, params.outputSecond);
        }
        frameTimer.restart();
    }

    frame = renderExportFrameSynchronously(exportBackend, *useOffscreenGpu, frameSize, params);
    if (frame.isNull()) {
        appendRenderBackendFallbackDetail(
            fallbackDetail,
            QStringLiteral("frame=%1 reason=quick_render_failed").arg(params.frameIndex));
        return ExportFrameRenderStatus::Failed;
    }

    readyFrames->push_back(buildReadyFramePayload(
        exportBackend,
        params.frameIndex,
        params.exportSecond,
        std::move(traceItems),
        std::move(frame),
        frameTimer.nsecsElapsed(),
        usedOffscreenPath || *useOffscreenGpu
    ));
    return ExportFrameRenderStatus::Ready;
}

bool drainPendingExportFrame(
    VideoExportQuickRenderBackend* exportBackend,
    std::deque<PendingPboFrame>* pendingPboFrames,
    const QSize& frameSize,
    ReadyFramePayload* readyFrame,
    QString* errorMessage
)
{
    if (exportBackend == nullptr || pendingPboFrames == nullptr || readyFrame == nullptr
        || pendingPboFrames->empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("no pending PBO frame to drain");
        }
        return false;
    }

    *readyFrame = ReadyFramePayload{};
    QElapsedTimer frameTimer;
    frameTimer.start();
    QImage drainedFrame;
    bool drainedFrameReady = false;
    QString drainError;
    const ExportFrameRenderParams& drainParams = pendingPboFrames->front().params;
    const bool drainOk = exportBackend->renderFrameOffscreenPboStep(
        frameSize,
        drainParams.exportSecond,
        drainParams.showTimestamp,
        drainParams.showObjectStatsHud,
        &drainedFrame,
        &drainedFrameReady,
        true,
        &drainError,
        drainParams.hudPlayheadSecondsOverride
    );
    if (!drainOk || !drainedFrameReady) {
        if (errorMessage != nullptr) {
            *errorMessage = drainError.isEmpty() ? QStringLiteral("failed to drain PBO readback") : drainError;
        }
        return false;
    }

    PendingPboFrame oldest = std::move(pendingPboFrames->front());
    pendingPboFrames->pop_front();
    *readyFrame = buildReadyFramePayload(
        exportBackend,
        oldest.params.frameIndex,
        oldest.params.exportSecond,
        std::move(oldest.traceItems),
        std::move(drainedFrame),
        frameTimer.nsecsElapsed(),
        true
    );
    return true;
}

bool preparePackedRgbaFrame(
    const QImage& frame,
    QImage* convertedFrame,
    QByteArray* packedScratch,
    const char** data,
    qint64* size
)
{
    if (convertedFrame == nullptr || packedScratch == nullptr || data == nullptr || size == nullptr) {
        return false;
    }
    *convertedFrame = QImage();
    packedScratch->clear();
    *data = nullptr;
    *size = 0;

    const QImage::Format expectedFormat = QImage::Format_RGBA8888_Premultiplied;
    const QImage* rgba = &frame;
    if (rgba->format() != expectedFormat) {
        *convertedFrame = frame.convertToFormat(expectedFormat);
        if (!frame.isNull() && convertedFrame->isNull()) {
            return false;
        }
        rgba = convertedFrame;
    }
    if (!frame.isNull() && rgba->isNull()) {
        return false;
    }

    const int width = rgba->width();
    const int height = rgba->height();
    if (width <= 0 || height <= 0) {
        return true;
    }

    const qint64 packedStride = static_cast<qint64>(width) * 4;
    const qint64 packedSize = packedStride * height;
    if (rgba->bytesPerLine() == packedStride) {
        if (packedSize > 0 && rgba->constBits() == nullptr) {
            return false;
        }
        *data = reinterpret_cast<const char*>(rgba->constBits());
        *size = packedSize;
        return true;
    }

    packedScratch->resize(static_cast<qsizetype>(packedSize));
    if (packedScratch->size() != packedSize || (packedSize > 0 && packedScratch->data() == nullptr)) {
        packedScratch->clear();
        return false;
    }
    for (int y = 0; y < height; ++y) {
        std::memcpy(
            packedScratch->data() + y * packedStride,
            rgba->constScanLine(y),
            static_cast<size_t>(packedStride)
        );
    }
    *data = packedScratch->constData();
    *size = packedScratch->size();
    return true;
}

}  // namespace miacode::video_export::detail
