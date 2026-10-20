#include "preview/stage_media/PreviewStageMediaHost.h"

#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "common/FileContentStamp.h"
#include "common/OperationLog.h"
#include "preview/stage_media/PreviewSharedD3D11Device.h"  // H2: single_device= log field
#include "core/video/PreviewEndOfMediaPolicy.h"  // §5.2 EndOfMedia classification

#include <cstdio>  // G2 Diag: std::snprintf for sync rate-change beacon lines

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi.h>          // F1: DXGI adapter probe to auto-detect integrated GPUs
#include <wrl/client.h>
#pragma comment(lib, "dxgi.lib")
#endif

// Windows preview decode backend: FFmpeg via QtAVPlayer. QT_AVPLAYER_MULTIMEDIA
// turns on the QAVVideoFrame -> QVideoFrame bridge (the conversion we feed to
// the QML VideoOutput sink). Still need QVideoFrame/QVideoSink for delivery.
#ifndef QT_AVPLAYER_MULTIMEDIA
#define QT_AVPLAYER_MULTIMEDIA
#endif
#include <QtAVPlayer/qavplayer.h>
#include <QtAVPlayer/qavvideoframe.h>
#include <QVideoFrame>
#include <QVideoSink>
#if defined(Q_OS_WIN)
#include <QtAVPlayer/qavd3d11sharedcontext_p.h>  // HW-decode diag counters / seek catch-up
#endif
#include <QtAVPlayer/qavpreviewdemuxdiag_p.h>  // §5.2 demuxer end-of-file provenance

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTimer>
#include <QVariant>
#include <QtMath>

#include "preview/stage_media/PreviewStageMediaHostInternal.h"

using namespace miacode::preview::psmh_detail;

namespace {

constexpr int kMediaSeekPrepareTimeoutMs = 800;

// Stale-EndOfMedia recovery budget (audit §5.2), counted per loaded media.
// The first attempts re-seek the live decoder — cheap, and enough to clear the
// AVIO end-of-file latch that turns one failed byte read into "the file ended".
// The last attempt re-opens the source. After that the PV keeps its last frame
// rather than looping, so a genuinely unreadable file degrades instead of
// thrashing the decoder for the rest of the chart.
constexpr int kStaleEndOfMediaMaxRecoveries = 3;
constexpr int kStaleEndOfMediaSeekAttempts = 2;
// How long the seek-based recovery waits for its `seeked` acknowledgement before
// escalating to a reload. Sized well above the observed seek-landing latency and
// well below anything a user would read as "the PV is stuck".
constexpr int kStaleEndOfMediaResumeTimeoutMs = 900;

// Audit §5.2 — the decisive fact about an EndOfMedia is WHY the demuxer decided the
// file ended. `avio_feof` latches after any failed byte read, so an eof that came from
// there with a non-zero AVIO error is an I/O failure wearing an end-of-stream costume.
// Returns a leading-space `key=val …` fragment, or an empty string off the FFmpeg path.
QString demuxEndOfFileDiagnosticFields()
{
    QAVPreviewDemuxEofDiag diag{};
    qavGetPreviewDemuxEofDiag(&diag);
    return QStringLiteral(" demux_eof_events=%1 demux_eof_averror=%2 demux_eof_avio=%3 "
                          "demux_read_failures=%4 demux_last_read_result=%5 "
                          "demux_last_avio_error=%6 demux_last_eof_byte_pos=%7 "
                          "demux_seek_resets=%8")
        .arg(diag.eofEvents)
        .arg(diag.eofFromAvErrorEof)
        .arg(diag.eofFromAvioFeof)
        .arg(diag.readFailures)
        .arg(diag.lastReadResult)
        .arg(diag.lastAvioError)
        .arg(diag.lastEofBytePos)
        .arg(diag.seekResets);
}

}  // namespace

void PreviewStageMediaHost::schedulePreparedPlaybackTimeout(quint64 transactionId, qint64 targetMs)
{
    const quint64 serial = ++preparedPlaybackTimeoutSerial_;
    QTimer::singleShot(kMediaSeekPrepareTimeoutMs, this, [this, serial, transactionId, targetMs]() {
        if (serial != preparedPlaybackTimeoutSerial_
            || !preparedPlaybackPending_
            || preparedPlaybackTransaction_ != transactionId
            || preparedPlaybackTargetMs_ != targetMs) {
            return;
        }
        const double targetSecond = preparedPlaybackTargetSecond_;
        appendPreviewStageMediaLog(
            QStringLiteral("prepare_playback_timeout"),
            QString("txn=%1 second=%2 target_ms=%3 position_ms=%4 frame_count=%5")
                .arg(transactionId)
                .arg(targetSecond, 0, 'f', 6)
                .arg(targetMs)
                .arg(player_ != nullptr ? player_->position() : -1)
                .arg(videoFrameCountTotal_));
        // If the ack frame has not arrived, settle the handshake so timeline
        // playback can start.
        preparedPlaybackPending_ = false;
        preparedPlaybackReady_ = true;
        preparedPlaybackLandingConfirmed_ = false;
        ++preparedPlaybackTimeoutSerial_;
        emit playbackStartPrepared(targetSecond, transactionId);
    });
}


void PreviewStageMediaHost::schedulePausedSeekTimeout(quint64 generation, qint64 targetMs)
{
    const quint64 serial = ++pausedSeekTimeoutSerial_;
    QTimer::singleShot(kMediaSeekPrepareTimeoutMs, this, [this, serial, generation, targetMs]() {
        if (serial != pausedSeekTimeoutSerial_
            || !pausedSeekCompletionPending_
            || pausedSeekGeneration_ != generation
            || pausedSeekTargetMs_ != targetMs) {
            return;
        }
        const double targetSecond = pausedSeekTargetSecond_;
        appendPreviewStageMediaLog(
            QStringLiteral("paused_seek_media_timeout"),
            QString("generation=%1 second=%2 target_ms=%3 position_ms=%4 frame_count=%5")
                .arg(generation)
                .arg(targetSecond, 0, 'f', 6)
                .arg(targetMs)
                .arg(player_ != nullptr ? player_->position() : -1)
                .arg(videoFrameCountTotal_));
        pausedSeekCompletionPending_ = false;
        ++pausedSeekTimeoutSerial_;
        emit pausedSeekCompleted(targetSecond, generation);
    });
}


void PreviewStageMediaHost::settlePendingSeekAcks(double mediaSecondStart, double mediaSecondEnd, bool frameDelivered)
{
    const qint64 startMs = qRound64(mediaSecondStart * 1000.0);
    const qint64 endMs = qRound64(qMax(mediaSecondStart, mediaSecondEnd) * 1000.0);
    const auto reaches = [&](qint64 targetMs) {
        return targetMs >= startMs - kPausedSeekAckToleranceMs
            && targetMs <= endMs + kPausedSeekAckToleranceMs;
    };
    if (frameDelivered && pausedSeekCompletionPending_ && pausedSeekTargetMs_ >= 0 && reaches(pausedSeekTargetMs_)) {
        pausedSeekCompletionPending_ = false;
        ++pausedSeekTimeoutSerial_;
        appendPreviewStageMediaLog(
            QStringLiteral("paused_seek_media_ack"),
            QString("generation=%1 second=%2 target_ms=%3 frame_start_ms=%4 frame_end_ms=%5 source=frame")
                .arg(pausedSeekGeneration_)
                .arg(pausedSeekTargetSecond_, 0, 'f', 6)
                .arg(pausedSeekTargetMs_)
                .arg(startMs)
                .arg(endMs));
        emit pausedSeekCompleted(pausedSeekTargetSecond_, pausedSeekGeneration_);
    }
    if (preparedPlaybackPending_ && preparedPlaybackTargetMs_ >= 0 && reaches(preparedPlaybackTargetMs_)) {
        preparedPlaybackPending_ = false;
        preparedPlaybackReady_ = true;
        preparedPlaybackLandingConfirmed_ = true;
        ++preparedPlaybackTimeoutSerial_;
        appendPreviewStageMediaLog(
            QStringLiteral("prepare_playback_ready"),
            QString("txn=%1 second=%2 target_ms=%3 frame_start_ms=%4 frame_end_ms=%5 source=frame")
                .arg(preparedPlaybackTransaction_)
                .arg(preparedPlaybackTargetSecond_, 0, 'f', 6)
                .arg(preparedPlaybackTargetMs_)
                .arg(startMs)
                .arg(endMs));
        emit playbackStartPrepared(preparedPlaybackTargetSecond_, preparedPlaybackTransaction_);
    }
}

// ---------------------------------------------------------------------------
// Stale EndOfMedia (audit §5.2)
// ---------------------------------------------------------------------------
// `EndOfMedia` is reported for two very different situations and the enum alone
// cannot tell them apart, so every decision here goes through
// core/video/PreviewEndOfMediaPolicy.h, which is covered by
// preview_end_of_media_policy_spec.
//
// Neither branch ends the main preview transport. The PV is subordinate
// visual media, and a natural end simply keeps its last
// frame while BGM / chart / timeline continue to the unified content endpoint.
void PreviewStageMediaHost::handleVideoEndOfMedia(bool wasPlaybackActive)
{
    using miacode::preview::video::classifyEndOfMedia;
    using miacode::preview::video::EndOfMediaClass;
    using miacode::preview::video::EndOfMediaFacts;
    using miacode::preview::video::endOfMediaClassName;
    using miacode::preview::video::endOfMediaShouldRecover;

    staleEndOfMediaResumePending_ = false;
    ++staleEndOfMediaResumeSerial_;

    EndOfMediaFacts facts;
    facts.durationSeconds = player_ != nullptr
        ? static_cast<double>(player_->duration()) / 1000.0
        : 0.0;
    // Decode progress, taken from the last frame the decoder actually produced.
    // NOT player_->position(): QAVPlayer::position() returns duration() outright
    // once mediaStatus() is EndOfMedia, so it reads "at the end" for a stale event
    // and a real one alike. (The handoff audit cites position_ms=121066 as evidence;
    // it is true by construction, and the pts below is the real evidence.)
    facts.decodedSeconds = lastFramePtsSeconds_;
    const double intervalMs = videoFrameIntervalAvgMs();
    facts.frameIntervalSeconds = intervalMs > 0.0 ? intervalMs / 1000.0 : 0.0;
    facts.expectedSeconds = qMax(0.0, observedPlayheadSecond_) + timelineOffsetSeconds_;

    const EndOfMediaClass classification = classifyEndOfMedia(facts);
    const bool recoverable = wasPlaybackActive
        && !shuttingDown_
        && mediaKind_ == MediaKind::Video
        && endOfMediaShouldRecover(facts);
    const bool budgetLeft = staleEndOfMediaRecoveries_ < kStaleEndOfMediaMaxRecoveries;

    appendPreviewStageMediaLog(
        QStringLiteral("end_of_media_classified"),
        QString("eom_class=%1 duration_ms=%2 decoded_ms=%3 expected_ms=%4 shortfall_ms=%5 "
                "frame_interval_ms=%6 frames=%7 was_active=%8 recoverable=%9 "
                "recoveries_used=%10 recovery_budget=%11 txn=%12 last_seek_ms=%13%14")
            .arg(QString::fromLatin1(endOfMediaClassName(classification)))
            .arg(qRound64(facts.durationSeconds * 1000.0))
            .arg(facts.decodedSeconds >= 0.0 ? qRound64(facts.decodedSeconds * 1000.0) : -1)
            .arg(qRound64(facts.expectedSeconds * 1000.0))
            .arg(facts.decodedSeconds >= 0.0
                     ? qRound64((facts.durationSeconds - facts.decodedSeconds) * 1000.0)
                     : -1)
            .arg(intervalMs, 0, 'f', 3)
            .arg(videoFrameCountTotal_)
            .arg(wasPlaybackActive ? 1 : 0)
            .arg(recoverable ? 1 : 0)
            .arg(staleEndOfMediaRecoveries_)
            .arg(kStaleEndOfMediaMaxRecoveries)
            .arg(playbackTransactionId_)
            .arg(lastSeekMs_)
            .arg(demuxEndOfFileDiagnosticFields()));

    if (classification != EndOfMediaClass::Stale || !recoverable) {
        return;
    }
    if (!budgetLeft) {
        appendPreviewStageMediaLog(
            QStringLiteral("stale_end_of_media_exhausted"),
            QString("recoveries=%1 limit=%2 expected_ms=%3 duration_ms=%4")
                .arg(staleEndOfMediaRecoveries_)
                .arg(kStaleEndOfMediaMaxRecoveries)
                .arg(qRound64(facts.expectedSeconds * 1000.0))
                .arg(qRound64(facts.durationSeconds * 1000.0)));
        return;
    }
    tryRecoverFromStaleEndOfMedia(qMax(0.0, observedPlayheadSecond_));
}

void PreviewStageMediaHost::resetStaleEndOfMediaRecovery()
{
    staleEndOfMediaRecoveries_ = 0;
    staleEndOfMediaResumePending_ = false;
    staleEndOfMediaResumeSecond_ = 0.0;
    ++staleEndOfMediaResumeSerial_;
}

bool PreviewStageMediaHost::tryRecoverFromStaleEndOfMedia(double targetSecond)
{
    if (shuttingDown_ || player_ == nullptr || mediaKind_ != MediaKind::Video
        || mediaPath_.isEmpty()) {
        return false;
    }
    if (staleEndOfMediaRecoveries_ >= kStaleEndOfMediaMaxRecoveries) {
        return false;
    }
    const double clampedSecond = qMax(0.0, qIsFinite(targetSecond) ? targetSecond : 0.0);
    const qint64 targetMs =
        qMax<qint64>(0, qRound64((clampedSecond + timelineOffsetSeconds_) * 1000.0));
    const int attempt = staleEndOfMediaRecoveries_++;
    const bool reload = attempt >= kStaleEndOfMediaSeekAttempts;

    appendPreviewStageMediaLog(
        QStringLiteral("stale_end_of_media_recover"),
        QString("attempt=%1 strategy=%2 target_second=%3 target_ms=%4 frames=%5 path=%6")
            .arg(attempt)
            .arg(reload ? QStringLiteral("reload") : QStringLiteral("seek_resume"))
            .arg(clampedSecond, 0, 'f', 6)
            .arg(targetMs)
            .arg(videoFrameCountTotal_)
            .arg(mediaPath_));

    if (reload) {
        // Re-opening runs QAVPlayer's terminate(), which clears the end-of-file
        // latch, so the play() below cannot be re-interpreted as "restart from the
        // beginning". Same in-place reload shape as the software-decode fallback.
        player_->stop();
        player_->setSource(QString());
        player_->setSource(mediaPath_);
        player_->setSpeed(static_cast<qreal>(playbackRate_));
        lastSeekMs_ = targetMs;
        player_->seek(targetMs);
        player_->play();
        videoPlaybackActive_ = true;
        videoPlaybackPendingStart_ = false;
        videoPlaybackActiveElapsed_.restart();
        if (videoFrameElapsed_.isValid()) {
            videoFrameElapsed_.restart();
        }
        updateClockDelta();
        emit diagnosticsChanged();
        return true;
    }

    // Seek first, resume on the acknowledgement. QAVPlayer::play() re-seeks to 0
    // whenever its end-of-file latch is still set, so playing before the seek lands
    // would restart the PV from the beginning — the very failure this recovers from.
    staleEndOfMediaResumePending_ = true;
    staleEndOfMediaResumeSecond_ = clampedSecond;
    const quint64 serial = ++staleEndOfMediaResumeSerial_;
    lastSeekMs_ = targetMs;
    player_->seek(targetMs);
    QTimer::singleShot(kStaleEndOfMediaResumeTimeoutMs, this, [this, serial, clampedSecond]() {
        if (serial != staleEndOfMediaResumeSerial_ || !staleEndOfMediaResumePending_) {
            return;
        }
        staleEndOfMediaResumePending_ = false;
        appendPreviewStageMediaLog(
            QStringLiteral("stale_end_of_media_resume_timeout"),
            QString("target_second=%1 recoveries=%2")
                .arg(clampedSecond, 0, 'f', 6)
                .arg(staleEndOfMediaRecoveries_));
        // Escalate: the seek never acknowledged, so the decoder needs re-opening.
        staleEndOfMediaRecoveries_ =
            qMax(staleEndOfMediaRecoveries_, kStaleEndOfMediaSeekAttempts);
        tryRecoverFromStaleEndOfMedia(clampedSecond);
    });
    return true;
}
