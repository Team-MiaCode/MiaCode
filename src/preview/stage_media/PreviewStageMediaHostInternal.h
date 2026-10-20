#pragma once

// Internal header for the PreviewStageMediaHost translation units.
//
// PreviewStageMediaHost.cpp was split into multiple .cpp TUs
// (PreviewStageMediaHost_Backend/Media/Playback/Diagnostics/Timeout.cpp +
// the slimmed PreviewStageMediaHost.cpp core). File-local helpers/constants
// that were in the original .cpp's anonymous namespace and are used by MORE
// THAN ONE of those TUs live here, in a NAMED namespace, so they have
// external linkage and a single definition instead of one copy per TU.
//
// STATELESS helpers only. Helpers/constants used by exactly one TU stay in
// that TU's own anonymous namespace. Mutable shared state (e.g. the
// isIntegratedRenderAdapter() static-local cache) is NOT here — it stays in
// exactly one TU (Backend) to avoid splitting the cache.
//
// Each TU includes this header (after the same #include block the original
// .cpp had) and does `using namespace miacode::preview::psmh_detail;`.

#include "common/DebugLog.h"

#include <QString>

namespace miacode {
namespace preview {
namespace psmh_detail {

inline constexpr qint64 kPausedSeekAckToleranceMs = 80;

// 播放控制中的相邻定位请求使用此容差；暂停拖动的复用依据为显示帧时间范围。
inline constexpr qint64 kSeekCoalesceToleranceMs = 40;

inline void appendPreviewStageMediaLog(const QString& action, const QString& payload = QString())
{
    QString text = QStringLiteral("action=%1").arg(action);
    if (!payload.trimmed().isEmpty()) {
        text += QStringLiteral(" ") + payload.trimmed();
    }
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Audio,
        QStringLiteral("preview/stage_media"),
        text
    );
}

}  // namespace psmh_detail
}  // namespace preview
}  // namespace miacode
