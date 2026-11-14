#pragma once

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringView>
#include <QVector>

namespace miacode::timeline {

enum class TimelineLaneOrder {
    Natural,
    Rotated,
    Reversed,
    RotatedReversed,
};

inline TimelineLaneOrder timelineLaneOrderForToken(QStringView token)
{
    if (token == u"rotated") return TimelineLaneOrder::Rotated;
    if (token == u"reversed") return TimelineLaneOrder::Reversed;
    if (token == u"rotated_reversed") return TimelineLaneOrder::RotatedReversed;
    return TimelineLaneOrder::Natural;
}

inline int timelineLaneForSlot(TimelineLaneOrder order, int slot)
{
    // The permutations are self-inverse and leave the Touch lane in place.
    if (slot < 1 || slot > 8) return slot;
    switch (order) {
    case TimelineLaneOrder::Natural: return slot;
    case TimelineLaneOrder::Rotated: return (slot + 3) % 8 + 1;
    case TimelineLaneOrder::Reversed: return 9 - slot;
    case TimelineLaneOrder::RotatedReversed: return (12 - slot) % 8 + 1;
    }
    return slot;
}

inline int timelineSlotForLane(TimelineLaneOrder order, int lane)
{
    return timelineLaneForSlot(order, lane);
}

struct TimelineSceneLayoutMetrics {
    QSize viewportSize;
    int timelineLeft = 0;
    int timelineTop = 0;
    qreal timelineHeight = 0.0;
    qreal laneHeight = 0.0;
    int laneCount = 0;
    int leadingCenteringPadding = 0;
    int trailingCenteringPadding = 0;
    int contentWidth = 0;
    double displayStartSeconds = 0.0;
    double displayEndSeconds = 0.0;
    double pixelsPerSecond = 0.0;
    double contentScale = 1.0;
    double maxNavigableSecond = 0.0;
};

enum class TimelineSceneGlyphShape {
    Circle,
    Diamond,
    RoundedRect,
};

struct TimelineSceneRect {
    QRectF rect;
    QColor color;
};

struct TimelineSceneLine {
    QPointF start;
    QPointF end;
    QColor color;
    qreal width = 1.0;
};

struct TimelineSceneTriangle {
    QPointF a;
    QPointF b;
    QPointF c;
    QColor color;
};

struct TimelineSceneTextLabel {
    QString text;
    QPointF topLeft;
    QFont font;
    QColor color;
    QSizeF logicalSize;
};

struct TimelineSceneGlyph {
    QRectF rect;
    TimelineSceneGlyphShape shape = TimelineSceneGlyphShape::Circle;
    QColor fillColor;
    QColor strokeColor;
    qreal strokeWidth = 0.0;
    qreal radius = 0.0;
    quint64 locationId = 0;
    QString tooltipText;
};

struct TimelineMuriMarkerPlacement {
    double second = 0.0;
    bool useTriggerSecond = false;
    QString tooltipText;
};

inline bool operator==(
    const TimelineMuriMarkerPlacement& left,
    const TimelineMuriMarkerPlacement& right)
{
    return qFuzzyCompare(left.second + 1.0, right.second + 1.0)
        && left.useTriggerSecond == right.useTriggerSecond
        && left.tooltipText == right.tooltipText;
}

struct TimelineSceneSprite {
    QPointF center;
    QString spriteType;
    qreal scale = 1.0;
    qreal rotationDegrees = 0.0;
    bool mirrorX = false;
};

struct TimelineSceneHoldSpan {
    int startX = 0;
    int endX = 0;
    qreal rowTop = 0.0;
    qreal laneHeight = 0.0;
    QString spriteType;
    qreal baseIconScale = 1.0;
    QColor fallbackColor;
    qreal fallbackWidth = 1.0;
};

struct TimelineSceneState {
    TimelineLaneOrder laneOrder = TimelineLaneOrder::Natural;
    QSize viewportSize;
    int timelineLeft = 0;
    int timelineTop = 0;
    qreal timelineHeight = 0.0;
    qreal laneHeight = 0.0;
    int laneCount = 0;
    // Sub-pixel — see TimelineSceneBuildRequest::horizontalScrollValue.
    double horizontalScrollValue = 0.0;
    int headerLeftLimit = 0;
    int headerRightLimit = 0;
    int headerMarkerLeftLimit = 0;
    int headerMarkerRightLimit = 0;
    int leadingCenteringPadding = 0;
    int trailingCenteringPadding = 0;
    int contentWidth = 0;
    double displayStartSeconds = 0.0;
    double displayEndSeconds = 0.0;
    double visibleStartSecond = 0.0;
    double visibleEndSecond = 0.0;
    double pixelsPerSecond = 0.0;
    double contentScale = 1.0;
    double maxNavigableSecond = 0.0;
    double waveformPhaseCompensationSeconds = 0.0;
    quint64 appearanceRevision = 0;
    quint64 layoutRevision = 0;
    quint64 gridRevision = 0;
    quint64 waveformRevision = 0;
    quint64 headerRevision = 0;
    quint64 notesRevision = 0;
    quint64 overlayRevision = 0;
    quint64 overlayDynamicRevision = 0;
    QString skinDirectory;
    QVector<TimelineSceneRect> baseBackgroundRects;
    QVector<TimelineSceneRect> laneOverlayRects;
    QVector<TimelineSceneRect> waveformBars;
    QVector<TimelineSceneLine> gridLines;
    QVector<TimelineSceneRect> frameRects;
    QVector<TimelineSceneLine> frameLines;
    QVector<TimelineSceneTextLabel> laneLabels;
    QVector<TimelineSceneTextLabel> headerLabels;
    QVector<TimelineSceneTriangle> headerMarkers;
    QVector<TimelineSceneRect> fireworkBands;
    QVector<TimelineSceneLine> trackLines;
    QVector<TimelineSceneSprite> trackSprites;
    QVector<TimelineSceneHoldSpan> holdSpans;
    QVector<TimelineSceneLine> touchHoldLines;
    QVector<TimelineSceneSprite> noteSprites;
    QVector<TimelineSceneGlyph> muriDots;
    bool hasEntryMarker = false;
    TimelineSceneTriangle entryMarker;
    bool hasCursorMarker = false;
    TimelineSceneTriangle cursorMarker;
    bool hasCursorLine = false;
    TimelineSceneLine cursorLine;
    bool hasPlayheadLine = false;
    TimelineSceneLine playheadLine;
    bool hasDragCenterLine = false;
    TimelineSceneLine dragCenterLine;

};

}  // namespace miacode::timeline
