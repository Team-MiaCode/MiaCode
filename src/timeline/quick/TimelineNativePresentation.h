#pragma once

#include <QtGlobal>
#include <memory>

class QQuickWindow;
class TimelineFramePresentationClock;

#ifdef Q_OS_MACOS
void observeTimelineNativePresentation(
    QQuickWindow* window, const std::shared_ptr<TimelineFramePresentationClock>& clock);
#endif
