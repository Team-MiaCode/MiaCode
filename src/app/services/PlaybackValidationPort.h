#pragma once

#include "core/analysis/MuriRenderOptions.h"

namespace miacode {

// 播放协调器通过此端口更新分析设置与校验结果。
class PlaybackValidationPort
{
public:
    virtual ~PlaybackValidationPort() = default;

    virtual void setMuriRenderMode(RenderMode mode, bool persistState) = 0;
    // 手部半径 / 撞尾阈值, in stored units (1080 px reference radius, milliseconds):
    // clamp, store, persist and re-run the muri analysis.
    virtual void setMuriHandRadiusPx(int radiusPx) = 0;
    virtual void setStaticTapOnSlideThresholdMs(int thresholdMs) = 0;
    virtual void clearValidationCache() = 0;
    virtual void clearValidationDecorations() = 0;
    virtual void applyAlignedMuriAnalysisReportToViews() = 0;
    virtual void applyDeferredAnalysisUiUpdates() = 0;
    virtual void notifyDocumentValidationChanged() = 0;
};

}  // namespace miacode
