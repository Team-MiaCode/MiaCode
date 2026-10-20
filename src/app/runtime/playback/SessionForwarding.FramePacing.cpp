
#include "app/runtime/playback/PlaybackCoordinator.h"
#include "app/runtime/Session.h"
#include "app/runtime/Shared.h"

void Session::setPreviewFixedTimerHighResolutionActive(bool active)
{
    miacode::runtime::shared::setPreviewFixedTimerHighResolutionActive(state_, active);
}

Session::PreviewCanvasFrameRateMode Session::previewFrameRateModeFromStorageValue(
    const QString& value,
    PreviewCanvasFrameRateMode fallback) const
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("30") || normalized == QLatin1String("30fps")) {
        return PreviewCanvasFrameRateMode::Fps30;
    }
    if (normalized == QLatin1String("60") || normalized == QLatin1String("60fps")) {
        return PreviewCanvasFrameRateMode::Fps60;
    }
    if (normalized == QLatin1String("120") || normalized == QLatin1String("120fps")) {
        return PreviewCanvasFrameRateMode::Fps120;
    }
    if (normalized == QLatin1String("display")
        || normalized == QLatin1String("display_max")
        || normalized == QLatin1String("screen")
        || normalized == QLatin1String("unlimited")) {
        return PreviewCanvasFrameRateMode::DisplayRefresh;
    }
    return fallback;
}

Session::PreviewCanvasFrameRateMode Session::previewCanvasFrameRateModeFromStorageValue(const QString& value) const
{
    return previewFrameRateModeFromStorageValue(value, PreviewCanvasFrameRateMode::Fps60);
}

Session::PreviewCanvasFrameRateMode Session::currentPreviewCanvasFrameRateMode() const
{
    return state_.previewCanvasFrameRateMode_;
}

Session::PreviewCanvasFrameRateMode Session::currentPreviewStageMediaFrameRateMode() const
{
    return playback_->currentPreviewStageMediaFrameRateMode();
}

bool Session::currentVideoDecodePrefersSoftware() const
{
    return playback_->currentVideoDecodePrefersSoftware();
}

Session::PreviewCanvasFrameRateMode Session::currentTimelineFrameRateMode() const
{
    return playback_->currentTimelineFrameRateMode();
}
