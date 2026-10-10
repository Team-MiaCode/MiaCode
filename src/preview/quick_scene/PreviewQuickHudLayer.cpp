#include "preview/quick_scene/PreviewQuickHudLayer.h"

#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "preview/runtime/PreviewRuntime.h"
#include "core/scene/PreviewFrameState.h"
#include "core/scene/PreviewHudState.h"
#include "core/scene/PreviewProgressStatsCache.h"
#include "core/scene/PreviewSceneGeometry.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintDevice>
#include <QPainterPath>
#include <QQuickWindow>
#include <QTimer>
#include <QTextLayout>

#include <array>
#include <cmath>
#include <memory>
#include <optional>

namespace miacode::preview::hud {

struct HudInformationLayoutKey {
    QRectF stage;
    QRectF playfield;
    QRectF timestamp;
    qreal chartTop = 0;
    qreal padding = 0;
    qreal shadow = 0;
    int minimumPointSize = 1;
    int logicalDpi = 96;
    QFont font;
    QString fontPath;
    QStringList samples;
    bool operator==(const HudInformationLayoutKey&) const = default;
};

struct HudInformationLayout {
    QFont font;
    QRectF bounds;
    QStringList lines;
    QVector<QPointF> baselines;
    bool compact = false;
};

struct HudInformationLayoutCache {
    std::optional<HudInformationLayoutKey> chartKey;
    std::optional<HudInformationLayoutKey> statsKey;
    HudInformationLayout chart;
    HudInformationLayout stats;
};

} // namespace miacode::preview::hud

namespace {

QString pointerHex(const void* pointer)
{
    return QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(pointer), 0, 16);
}

QString logTextPreview(QString text)
{
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    text.replace(QLatin1Char('\t'), QLatin1Char(' '));
    text.replace(QLatin1Char('"'), QLatin1Char('\''));
    constexpr int kMaxPreviewChars = 96;
    if (text.size() > kMaxPreviewChars) {
        text = text.left(kMaxPreviewChars) + QStringLiteral("...");
    }
    return text;
}

QString painterDiagPayload(QPainter& painter)
{
    const QPaintDevice* device = painter.device();
    return QStringLiteral("painter=%1 active=%2 device=%3 device_size=%4x%5 device_dpr=%6")
        .arg(pointerHex(&painter))
        .arg(painter.isActive() ? 1 : 0)
        .arg(pointerHex(device))
        .arg(device != nullptr ? device->width() : -1)
        .arg(device != nullptr ? device->height() : -1)
        .arg(device != nullptr ? device->devicePixelRatioF() : 0.0, 0, 'f', 3);
}

// MIACODE_PREVIEW_HUD_PAINT_DIAG is a launch-time diagnostic switch, so read it
// ONCE instead of hitting the global environment lock on every diag site (there
// are ~20 per HUD paint). Same caching rationale as skipAsyncLogFlush() in
// DebugLog.cpp.
bool hudPaintDiagEnabled()
{
    static const bool value = miacode::debug_options::previewHudPaintDiagnosticsEnabled();
    return value;
}

void appendHudPaintDiagLine(const QString& action, const QString& detail = QString(), bool durable = false)
{
    if (!hudPaintDiagEnabled()) {
        return;
    }
    QString payload = QStringLiteral("action=%1").arg(action);
    if (!detail.trimmed().isEmpty()) {
        payload += QStringLiteral(" ") + detail.trimmed();
    }
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Runtime,
        QStringLiteral("preview/hud_paint"),
        payload,
        /*force=*/true);
    if (durable) {
        miacode::debug_log::flushAsyncLogWriter(100);
    }
}

// The diag DETAIL string is the expensive half: pointerHex / painterDiagPayload /
// logTextPreview plus long .arg() chains. Built by the caller, it used to be
// formatted unconditionally and then dropped on the floor inside
// appendHudPaintDiagLine when the switch was off — a fixed per-paint allocation
// cost every user paid for a diagnostic almost nobody runs. Every site now goes
// through this lazy wrapper, so with the switch off the detail is never built.
template <typename DetailFn>
void appendHudPaintDiag(const QString& action, DetailFn&& detailFn, bool durable = false)
{
    if (!hudPaintDiagEnabled()) {
        return;
    }
    appendHudPaintDiagLine(action, detailFn(), durable);
}

void drawHudTextWithColors(
    QPainter& painter,
    const QString& tag,
    const QPointF& baseline,
    const QString& text,
    const QFont& font,
    qreal shadowOffset,
    const QColor& textColor,
    const QColor& shadowColor)
{
    appendHudPaintDiag(
        QStringLiteral("draw_text_before"),
        [&] {
            return QStringLiteral(
                "tag=%1 baseline=%2,%3 text_len=%4 text_preview=\"%5\" font_family=\"%6\" font_point=%7 font_pixel=%8 font_weight=%9 shadow_offset=%10 %11")
                .arg(tag)
                .arg(baseline.x(), 0, 'f', 2)
                .arg(baseline.y(), 0, 'f', 2)
                .arg(text.size())
                .arg(logTextPreview(text))
                .arg(font.family())
                .arg(font.pointSize())
                .arg(font.pixelSize())
                .arg(font.weight())
                .arg(shadowOffset, 0, 'f', 2)
                .arg(painterDiagPayload(painter));
        },
        /*durable=*/true);
    painter.save();
    painter.setFont(font);
    painter.setPen(shadowColor);
    painter.drawText(baseline + QPointF(shadowOffset, shadowOffset), text);
    painter.setPen(textColor);
    painter.drawText(baseline, text);
    painter.restore();
    appendHudPaintDiag(
        QStringLiteral("draw_text_after"),
        [&] {
            return QStringLiteral("tag=%1 text_len=%2 %3")
                .arg(tag)
                .arg(text.size())
                .arg(painterDiagPayload(painter));
        });
}

struct HudGlyphVerticalBounds {
    qreal top = 0.0;
    qreal bottom = 0.0;
    bool valid = false;
};

HudGlyphVerticalBounds hudGlyphVerticalBounds(const QFontMetrics& metrics, const QStringList& lines)
{
    HudGlyphVerticalBounds bounds;
    for (const QString& line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        const QRect glyphRect = metrics.boundingRect(line);
        if (!bounds.valid) {
            bounds.top = glyphRect.top();
            bounds.bottom = glyphRect.bottom();
            bounds.valid = true;
        } else {
            bounds.top = qMin(bounds.top, static_cast<qreal>(glyphRect.top()));
            bounds.bottom = qMax(bounds.bottom, static_cast<qreal>(glyphRect.bottom()));
        }
    }
    return bounds;
}

qreal hudLineAdvance(const QFontMetrics& metrics, const QStringList& lines)
{
    const HudGlyphVerticalBounds bounds = hudGlyphVerticalBounds(metrics, lines);
    const qreal glyphSpan = bounds.valid ? bounds.bottom - bounds.top + 1.0 : 0.0;
    return qMax(static_cast<qreal>(metrics.lineSpacing()), glyphSpan);
}

using miacode::preview::hud::HudInformationLayout;
using miacode::preview::hud::HudInformationLayoutKey;

// Measure each text row against the circle so wider corner space can be used
// without making the entire panel inherit its narrowest row's width.
QRectF informationRowRect(const HudInformationLayoutKey& key, qreal top, qreal height, bool bottomRight)
{
    const QRectF safe = key.stage.adjusted(key.padding, key.padding,
        -key.padding - key.shadow, -key.padding - key.shadow);
    const qreal bottom = top + height;
    if (height <= 0 || top < safe.top() || bottom > safe.bottom()
        || (!bottomRight && bottom > key.stage.center().y())) {
        return {};
    }
    const QPointF center = key.playfield.center();
    const qreal radius = key.playfield.width() / 2 + key.padding;
    const qreal closestY = qBound(top, center.y(), bottom);
    const qreal distanceY = qAbs(center.y() - closestY);
    const qreal circleHalfWidth = std::sqrt(qMax<qreal>(0, radius * radius - distanceY * distanceY));
    const qreal innerEdge = center.x() + (bottomRight ? circleHalfWidth : -circleHalfWidth);
    const qreal width = bottomRight ? safe.right() - innerEdge : innerEdge - safe.left();
    if (width <= 0) {
        return {};
    }
    return QRectF(bottomRight ? safe.right() - width : safe.left(), top, width, height);
}

qreal informationTextWidth(const QFontMetrics& metrics, const QString& text)
{
    const QRect glyphs = metrics.boundingRect(text);
    return qMax<qreal>(metrics.horizontalAdvance(text), glyphs.right() + 1)
        - qMin(0, glyphs.left());
}

qreal informationLeftBearing(const QFontMetrics& metrics, const QStringList& lines)
{
    qreal bearing = 0;
    for (const QString& line : lines) {
        if (!line.isEmpty()) bearing = qMax<qreal>(bearing, -metrics.boundingRect(line).left());
    }
    return bearing;
}

HudInformationLayout layoutChartInformation(const HudInformationLayoutKey& key, QPainter& painter)
{
    for (const bool compact : {false, true}) {
        QStringList fields = key.samples;
        if (compact) {
            fields = {key.samples.value(0), key.samples.value(2), QString(), QString()};
        }
        for (int size = key.font.pointSize(); size >= key.minimumPointSize; --size) {
            QFont font = key.font;
            font.setPointSize(size);
            const QFontMetrics metrics(font, painter.device());
            const auto glyphs = hudGlyphVerticalBounds(metrics, fields);
            const qreal ascent = qMax<qreal>(metrics.ascent(), glyphs.valid ? -glyphs.top : 0);
            const qreal descent = qMax<qreal>(metrics.descent(), glyphs.valid ? glyphs.bottom + 1 : 0);
            const qreal advance = hudLineAdvance(metrics, fields);
            const qreal bearing = informationLeftBearing(metrics, fields);
            qreal top = qMax(key.stage.top() + key.padding, key.chartTop);
            HudInformationLayout layout;
            layout.font = font;
            layout.compact = compact;
            bool fits = true;
            const auto appendLine = [&](const QString& text, const QRectF& available) {
                const qreal width = informationTextWidth(metrics, text) + bearing;
                const QRectF bounds(available.left(), top, width, ascent + descent);
                if (bounds.adjusted(0, 0, key.shadow, key.shadow).intersects(key.timestamp)) {
                    return false;
                }
                layout.lines.append(text);
                layout.baselines.append(QPointF(available.left() + bearing, top + ascent));
                layout.bounds = layout.bounds.united(bounds);
                top += advance;
                return true;
            };
            for (int i = 0; i < fields.size() && fits; ++i) {
                if (fields[i].isEmpty()) continue;
                if (i == 3) {
                    QTextLayout designer(fields[i], font, painter.device());
                    QTextOption option;
                    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
                    designer.setTextOption(option);
                    designer.beginLayout();
                    for (int row = 0; row < 2 && fits; ++row) {
                        QTextLine line = designer.createLine();
                        if (!line.isValid()) break;
                        const QRectF available = informationRowRect(key, top, ascent + descent, false);
                        const int width = qFloor(available.width() - bearing);
                        if (width < informationTextWidth(metrics, QStringLiteral("…")) * 3) {
                            fits = false;
                            break;
                        }
                        line.setLineWidth(width);
                        const QString text = row == 1
                            ? metrics.elidedText(fields[i].mid(line.textStart()), Qt::ElideRight, width)
                            : fields[i].mid(line.textStart(), line.textLength()).trimmed();
                        fits = appendLine(text, available);
                    }
                    designer.endLayout();
                } else {
                    const QRectF available = informationRowRect(key, top, ascent + descent, false);
                    const int width = qFloor(available.width() - bearing);
                    if (width < informationTextWidth(metrics, QStringLiteral("…")) * 3) {
                        fits = false;
                        break;
                    }
                    fits = appendLine(metrics.elidedText(fields[i], Qt::ElideRight, width), available);
                }
            }
            if (fits && !layout.lines.isEmpty()) return layout;
        }
    }
    return {};
}

HudInformationLayout layoutObjectStatistics(const HudInformationLayoutKey& key, QPainter& painter)
{
    for (int size = key.font.pointSize(); size >= key.minimumPointSize; --size) {
        QFont font = key.font;
        font.setPointSize(size);
        const QFontMetrics metrics(font, painter.device());
        for (const bool compact : {false, true}) {
            QStringList lines = key.samples;
            if (compact) {
                lines[0] = QStringLiteral("FiNALE");
                lines[2] = QStringLiteral("DELUXE");
            }
            const auto glyphs = hudGlyphVerticalBounds(metrics, lines);
            const qreal ascent = qMax<qreal>(metrics.ascent(), glyphs.valid ? -glyphs.top : 0);
            const qreal descent = qMax<qreal>(metrics.descent(), glyphs.valid ? glyphs.bottom + 1 : 0);
            const qreal advance = hudLineAdvance(metrics, lines);
            const qreal bearing = informationLeftBearing(metrics, lines);
            const int rows = compact ? 5 : lines.size();
            const qreal height = ascent + descent + (rows - 1) * advance;
            const qreal top = key.stage.bottom() - key.padding - key.shadow - height;
            const qreal right = key.stage.right() - key.padding - key.shadow;
            std::array<qreal, 2> columnWidths = {0, 0};
            const auto rowFor = [compact](int i) {
                return compact ? (i < 4 ? i % 2 : 2 + (i - 4) / 2) : i;
            };
            const auto columnFor = [compact](int i) {
                return compact ? (i < 4 ? i / 2 : (i - 4) % 2) : 0;
            };
            for (int i = 0; i < lines.size(); ++i) {
                const qreal width = informationTextWidth(metrics, lines[i]) + bearing;
                const int column = columnFor(i);
                columnWidths[column] = qMax(columnWidths[column], width);
            }
            const qreal blockWidth = compact
                ? columnWidths[0] + key.padding + columnWidths[1]
                : columnWidths[0];
            const qreal left = right - blockWidth;
            HudInformationLayout layout;
            layout.font = font;
            layout.compact = compact;
            bool fits = true;
            for (int row = 0; row < rows; ++row) {
                const QRectF available = informationRowRect(key, top + row * advance, ascent + descent, true);
                const QRectF bounds(left, top + row * advance, blockWidth, ascent + descent);
                if (available.isEmpty() || blockWidth > available.width()
                    || bounds.adjusted(0, 0, key.shadow, key.shadow).intersects(key.timestamp)) {
                    fits = false;
                    break;
                }
                layout.bounds = layout.bounds.united(bounds);
            }
            if (!fits) continue;
            for (int i = 0; i < lines.size(); ++i) {
                const int row = rowFor(i);
                const qreal columnOffset = columnFor(i) > 0 ? columnWidths[0] + key.padding : 0;
                layout.baselines.append(QPointF(left + bearing + columnOffset,
                    top + ascent + row * advance));
            }
            return layout;
        }
    }
    return {};
}

void drawOutlinedHudText(
    QPainter& painter,
    const QPointF& baseline,
    const QString& text,
    const QFont& font,
    qreal outlineWidth,
    const QPointF& shadowOffset,
    const QColor& fillColor)
{
    QPainterPath path;
    path.addText(baseline, font, text);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // Soft, slightly-dilated black drop shadow — approximates MajdataPlay's
    // CenterInfoDisplayer TMP underlay (Jua-Regular SDF material: _UnderlayColor
    // black, offset (0.59, -0.86) => down-right, _UnderlaySoftness 1, _UnderlayDilate 1).
    // Filled+stroked offset glyph copy mimics the dilated, softened underlay better
    // than an outline-only stroke.
    QPainterPath shadowPath;
    shadowPath.addText(baseline + shadowOffset, font, text);
    const QColor shadowColor(0, 0, 0, 160);
    painter.setPen(QPen(shadowColor, outlineWidth * 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(shadowColor);
    painter.drawPath(shadowPath);
    // Neutral-grey rim — matches TMP _OutlineColor rgb(185,185,185) at full opacity (a=1).
    painter.setPen(QPen(QColor(185, 185, 185, 255), outlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawPath(path);
    painter.restore();
}

QString formatCenterAchievement(double value)
{
    // Match MajdataPlay: truncate (floor) to 4 decimals, never round up.
    const double truncated = std::floor(value * 10000.0) / 10000.0;
    return QStringLiteral("%1%").arg(QString::number(truncated, 'f', 4));
}

QString centerDisplayTitle(miacode::preview_gameplay::CenterDisplayMode mode)
{
    switch (mode) {
    case miacode::preview_gameplay::CenterDisplayMode::Combo:
        return QStringLiteral("COMBO");
    case miacode::preview_gameplay::CenterDisplayMode::AchievementFinalePlus:
        return QStringLiteral("ACHIEVEMENT FiNALE");
    case miacode::preview_gameplay::CenterDisplayMode::AchievementDxPlus:
    case miacode::preview_gameplay::CenterDisplayMode::AchievementDxMinus100:
    case miacode::preview_gameplay::CenterDisplayMode::AchievementDxMinus101:
        return QStringLiteral("ACHIEVEMENT");
    case miacode::preview_gameplay::CenterDisplayMode::DxScorePlus:
    case miacode::preview_gameplay::CenterDisplayMode::DxScoreMinus:
        return QStringLiteral("DX SCORE");
    case miacode::preview_gameplay::CenterDisplayMode::Off:
    default:
        return QString();
    }
}

QString centerDisplayValue(
    miacode::preview_gameplay::CenterDisplayMode mode,
    const miacode::preview::scene::PreviewHudStats& stats)
{
    switch (mode) {
    case miacode::preview_gameplay::CenterDisplayMode::Combo:
        // MajdataPlay hides the whole displayer while combo is 0.
        return stats.combo == 0 ? QString() : QString::number(stats.combo);
    case miacode::preview_gameplay::CenterDisplayMode::AchievementDxPlus:
        return formatCenterAchievement(stats.deluxeRate);
    case miacode::preview_gameplay::CenterDisplayMode::AchievementDxMinus100:
        if (stats.deluxeBreakTotal > 0)
            return formatCenterAchievement(100.0 + static_cast<double>(stats.deluxeBreakCurrent) / stats.deluxeBreakTotal);
        return formatCenterAchievement(100.0);
    case miacode::preview_gameplay::CenterDisplayMode::AchievementDxMinus101:
        return formatCenterAchievement(101.0);
    case miacode::preview_gameplay::CenterDisplayMode::DxScorePlus:
        return QString::number(stats.dxScore);
    case miacode::preview_gameplay::CenterDisplayMode::DxScoreMinus:
        return QString::number(stats.dxScoreMax);
    case miacode::preview_gameplay::CenterDisplayMode::AchievementFinalePlus:
        return formatCenterAchievement(stats.finaleRate);
    case miacode::preview_gameplay::CenterDisplayMode::Off:
    default:
        return QString();
    }
}

// MajdataPlay's ObjectCounter color palette (achievement tiers via UpdateAchievementColor).
QColor achievementTierColor(double rate)
{
    if (rate >= 100.0) {
        return QColor(200, 159, 109);  // gold
    }
    if (rate >= 97.0) {
        return QColor(159, 159, 159);  // silver
    }
    if (rate >= 80.0) {
        return QColor(127, 48, 32);  // bronze
    }
    return QColor(63, 127, 176);  // dud / blue
}

QColor centerDisplayValueColor(
    miacode::preview_gameplay::CenterDisplayMode mode,
    const miacode::preview::scene::PreviewHudStats& stats)
{
    using Mode = miacode::preview_gameplay::CenterDisplayMode;
    switch (mode) {
    case Mode::Combo:
        return QColor(186, 62, 118);
    case Mode::AchievementDxPlus:
        return achievementTierColor(stats.deluxeRate);
    case Mode::AchievementDxMinus100:
        return achievementTierColor(stats.deluxeBreakTotal > 0
            ? 100.0 + static_cast<double>(stats.deluxeBreakCurrent) / stats.deluxeBreakTotal
            : 100.0);
    case Mode::AchievementDxMinus101:
        return achievementTierColor(101.0);
    case Mode::DxScorePlus:
    case Mode::DxScoreMinus:
        return QColor(63, 176, 63);
    case Mode::AchievementFinalePlus:
        return achievementTierColor(stats.finaleRate);
    case Mode::Off:
    default:
        return QColor(186, 62, 118);
    }
}

QColor centerDisplayHeaderColor(miacode::preview_gameplay::CenterDisplayMode mode)
{
    using Mode = miacode::preview_gameplay::CenterDisplayMode;
    switch (mode) {
    case Mode::DxScorePlus:
    case Mode::DxScoreMinus:
        return QColor(63, 176, 63);
    case Mode::AchievementDxPlus:
    case Mode::AchievementDxMinus100:
    case Mode::AchievementDxMinus101:
    case Mode::AchievementFinalePlus:
        return QColor(200, 159, 109);
    case Mode::Combo:
    case Mode::Off:
    default:
        return QColor(186, 62, 118);
    }
}

}  // namespace

// Min interval between HUD repaints. The HUD shows FPS/max-ms/stutter
// counts which are statistical aggregates over a ~1s rolling window —
// updating their visual presentation at ~10Hz is indistinguishable to the
// eye from updating at 60Hz, but cuts the QSG sync cost by 6× because the
// QQuickPaintedItem only re-rasterises its full-area backing texture on
// actual update() calls. Empirically observed to drop the QSG sync phase
// from ~7.8ms/frame to ~2-3ms on the test chart.
constexpr qint64 kHudUpdateIntervalMs = 100;

PreviewQuickHudLayer::PreviewQuickHudLayer(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , informationLayoutCache_(std::make_unique<miacode::preview::hud::HudInformationLayoutCache>())
{
    setOpaquePainting(false);
    setAntialiasing(true);
    hudUpdateThrottleTimer_.start();
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow* currentWindow) {
        graphicsInfoWindow_ = currentWindow;
        graphicsInfo_ = {};
        graphicsInfoReady_ = false;
        if (runtime_ != nullptr) {
            runtime_->setFrameSize(boundingRect().size().toSize());
        }
        update();
    });
}

PreviewQuickHudLayer::~PreviewQuickHudLayer() = default;

void PreviewQuickHudLayer::requestThrottledUpdate()
{
    if (!hudUpdateThrottleTimer_.isValid()) {
        hudUpdateThrottleTimer_.start();
    }
    const qint64 nowMs = hudUpdateThrottleTimer_.elapsed();
    if (lastHudUpdateMs_ < 0 || (nowMs - lastHudUpdateMs_) >= kHudUpdateIntervalMs) {
        lastHudUpdateMs_ = nowMs;
        hudUpdatePending_ = false;
        update();
        return;
    }
    // Inside the throttle window — schedule a single deferred update so the
    // last frameStateChanged in the window still produces a visible refresh
    // (otherwise a burst of changes ending at the start of the window would
    // never repaint until the next frameStateChanged after the window
    // closes, leaving stale text on screen).
    if (!hudUpdatePending_) {
        hudUpdatePending_ = true;
        const qint64 delayMs = kHudUpdateIntervalMs - (nowMs - lastHudUpdateMs_);
        QTimer::singleShot(qMax<qint64>(1, delayMs), this, [this]() {
            if (!hudUpdatePending_) {
                return;
            }
            lastHudUpdateMs_ = hudUpdateThrottleTimer_.elapsed();
            hudUpdatePending_ = false;
            update();
        });
    }
}

void PreviewQuickHudLayer::setRuntime(PreviewRuntime* runtime)
{
    if (runtime_ == runtime) {
        return;
    }
    if (runtimeUpdateConnection_) {
        QObject::disconnect(runtimeUpdateConnection_);
    }
    runtime_ = runtime;
    if (runtime_ != nullptr) {
        frameState_ = nullptr;
        runtimeUpdateConnection_ = QObject::connect(runtime_, &PreviewRuntime::frameStateChanged, this, [this]() {
            requestThrottledUpdate();
        });
        runtime_->setFrameSize(boundingRect().size().toSize());
    }
    emit runtimeChanged();
    update();
}

QObject* PreviewQuickHudLayer::runtimeObject() const
{
    return runtime_;
}

void PreviewQuickHudLayer::setRuntimeObject(QObject* runtimeObject)
{
    setRuntime(qobject_cast<PreviewRuntime*>(runtimeObject));
}

void PreviewQuickHudLayer::setFrameState(const miacode::preview::scene::PreviewFrameState* frameState)
{
    frameState_ = frameState;
    if (frameState_ != nullptr) {
        runtime_ = nullptr;
    }
    update();
}

void PreviewQuickHudLayer::setLayerFlags(miacode::preview::scene::PreviewRenderLayerFlags layerFlags)
{
    if (layerFlags_ == layerFlags) {
        return;
    }
    layerFlags_ = layerFlags;
    update();
}

QColor PreviewQuickHudLayer::textColor() const
{
    return textColor_;
}

void PreviewQuickHudLayer::setTextColor(const QColor& color)
{
    if (textColor_ == color) {
        return;
    }
    textColor_ = color;
    emit textColorChanged();
    update();
}

QColor PreviewQuickHudLayer::shadowColor() const
{
    return shadowColor_;
}

void PreviewQuickHudLayer::setShadowColor(const QColor& color)
{
    if (shadowColor_ == color) {
        return;
    }
    shadowColor_ = color;
    emit shadowColorChanged();
    update();
}

void PreviewQuickHudLayer::paint(QPainter* painter)
{
    if (painter == nullptr) {
        appendHudPaintDiag(
            QStringLiteral("paint_skip"),
            [&] {
                return QStringLiteral("reason=null_painter item=%1 runtime=%2 frame_state_member=%3")
                    .arg(pointerHex(this))
                    .arg(pointerHex(runtime_.data()))
                    .arg(pointerHex(frameState_));
            });
        return;
    }
    const QSize canvasSize = boundingRect().size().toSize();
    appendHudPaintDiag(
        QStringLiteral("paint_enter"),
        [&] {
            return QStringLiteral(
                "item=%1 runtime=%2 frame_state_member=%3 canvas=%4x%5 layer_flags=0x%6 %7")
                .arg(pointerHex(this))
                .arg(pointerHex(runtime_.data()))
                .arg(pointerHex(frameState_))
                .arg(canvasSize.width())
                .arg(canvasSize.height())
                .arg(layerFlags_, 0, 16)
                .arg(painterDiagPayload(*painter));
        });
    const miacode::preview::scene::PreviewFrameState* state = nullptr;
    QString stateSource = QStringLiteral("member");
    std::shared_ptr<const miacode::preview::scene::PreviewFrameState> runtimeStateSnapshot;
    if (runtime_ != nullptr) {
        runtimeStateSnapshot = runtime_->frameStateSnapshot();
        state = runtimeStateSnapshot.get();
        stateSource = QStringLiteral("runtime_snapshot");
    } else {
        state = frameState_;
    }
    if (state == nullptr) {
        appendHudPaintDiag(
            QStringLiteral("paint_skip"),
            [&] {
                return QStringLiteral("reason=null_state item=%1 runtime=%2 frame_state_member=%3")
                    .arg(pointerHex(this))
                    .arg(pointerHex(runtime_.data()))
                    .arg(pointerHex(frameState_));
            });
        return;
    }
    appendHudPaintDiag(
        QStringLiteral("paint_overlay_call"),
        [&] {
            return QStringLiteral(
                "item=%1 state=%2 state_source=%3 canvas=%4x%5 show_timestamp=%6 show_debug=%7 show_object_stats=%8 show_chart_info=%9 chart_title_len=%10 chart_artist_len=%11 chart_diff_len=%12 chart_designer_len=%13 progress_stats=%14 playhead=%15 hud_playhead_override=%16")
                .arg(pointerHex(this))
                .arg(pointerHex(state))
                .arg(stateSource)
                .arg(canvasSize.width())
                .arg(canvasSize.height())
                .arg(state->render.showTimestamp ? 1 : 0)
                .arg(state->render.showDebugInfo ? 1 : 0)
                .arg(state->render.showObjectStatsHud ? 1 : 0)
                .arg(state->render.showChartInfoHud ? 1 : 0)
                .arg(state->chartTitle.size())
                .arg(state->chartArtist.size())
                .arg(state->chartDifficultyLabel.size())
                .arg(state->chartDesigner.size())
                .arg(pointerHex(state->progressStatsCache.get()))
                .arg(state->playheadSeconds, 0, 'f', 3)
                .arg(state->hudPlayheadSecondsOverride, 0, 'f', 3);
        },
        /*durable=*/true);
    if (!graphicsInfoReady_ || graphicsInfoWindow_ != window()) {
        graphicsInfoWindow_ = window();
        graphicsInfo_ = miacode::preview::quick_scene::queryQuickGraphicsInfo(window());
        graphicsInfoReady_ = true;
    }
    miacode::preview::hud::paintPreviewHudOverlay(
        *painter, *state, canvasSize, layerFlags_, graphicsInfo_, textColor_, shadowColor_, informationLayoutCache_.get());
    appendHudPaintDiag(
        QStringLiteral("paint_exit"),
        [&] {
            return QStringLiteral("item=%1 state=%2 canvas=%3x%4")
                .arg(pointerHex(this))
                .arg(pointerHex(state))
                .arg(canvasSize.width())
                .arg(canvasSize.height());
        });
}

namespace miacode::preview::hud {

void paintPreviewHudOverlay(
    QPainter& painter,
    const miacode::preview::scene::PreviewFrameState& stateRef,
    const QSize& canvasSize,
    miacode::preview::scene::PreviewRenderLayerFlags layerFlags,
    const miacode::preview::quick_scene::QuickGraphicsInfo& graphicsInfo,
    const QColor& textColor,
    const QColor& shadowColor,
    HudInformationLayoutCache* informationLayoutCache)
{
    QRectF debugRegion;
    QRectF timestampRegion;
    const auto* state = &stateRef;
    const auto drawHudText = [&](QPainter& target,
                                 const QString& tag,
                                 const QPointF& baseline,
                                 const QString& text,
                                 const QFont& font,
                                 qreal shadowOffset) {
        drawHudTextWithColors(
            target, tag, baseline, text, font, shadowOffset, textColor, shadowColor);
        if (tag.startsWith(QLatin1String("debug."))) {
            const QFontMetrics metrics(font, target.device());
            const QRectF bounds = QRectF(metrics.boundingRect(text)).translated(baseline)
                .adjusted(0, 0, shadowOffset, shadowOffset);
            debugRegion = debugRegion.united(bounds);
        } else if (tag == QLatin1String("timestamp")) {
            const QFontMetrics metrics(font, target.device());
            timestampRegion = QRectF(metrics.boundingRect(QStringLiteral("-88:88:888")))
                .translated(baseline).adjusted(0, 0, shadowOffset, shadowOffset);
        }
    };
    appendHudPaintDiag(
        QStringLiteral("overlay_enter"),
        [&] {
            return QStringLiteral(
                "state=%1 canvas=%2x%3 layer_flags=0x%4 show_timestamp=%5 show_debug=%6 show_object_stats=%7 show_chart_info=%8 fix_hud_text_layout=%9 center_mode=%10 chart_title_len=%11 chart_artist_len=%12 chart_diff_len=%13 chart_designer_len=%14 progress_stats=%15 %16")
                .arg(pointerHex(state))
                .arg(canvasSize.width())
                .arg(canvasSize.height())
                .arg(layerFlags, 0, 16)
                .arg(state->render.showTimestamp ? 1 : 0)
                .arg(state->render.showDebugInfo ? 1 : 0)
                .arg(state->render.showObjectStatsHud ? 1 : 0)
                .arg(state->render.showChartInfoHud ? 1 : 0)
                .arg(state->render.fixHudTextLayout ? 1 : 0)
                .arg(static_cast<int>(state->render.centerDisplayMode))
                .arg(state->chartTitle.size())
                .arg(state->chartArtist.size())
                .arg(state->chartDifficultyLabel.size())
                .arg(state->chartDesigner.size())
                .arg(pointerHex(state->progressStatsCache.get()))
                .arg(painterDiagPayload(painter));
        });
    if (!miacode::preview::scene::previewRenderLayerEnabled(
            layerFlags, miacode::preview::scene::HudLayer)) {
        appendHudPaintDiag(
            QStringLiteral("overlay_skip"),
            [&] {
                return QStringLiteral("reason=hud_layer_disabled state=%1 layer_flags=0x%2")
                    .arg(pointerHex(state))
                    .arg(layerFlags, 0, 16);
            });
        return;
    }
    if (!state->render.showTimestamp
        && !state->render.showDebugInfo
        && !state->render.showObjectStatsHud
        && !state->render.showChartInfoHud) {
        appendHudPaintDiag(
            QStringLiteral("overlay_skip"),
            [&] {
                return QStringLiteral("reason=all_hud_disabled state=%1").arg(pointerHex(state));
            });
        return;
    }

    const QRectF stageRect = miacode::preview::scene::stageRectForSize(canvasSize);
    constexpr qreal kHudReferenceShortSide = 1024.0;
    constexpr qreal kHudReferencePadding = 18.0;
    constexpr int kHudReferenceDebugFontPointSize = 13;
    constexpr int kHudReferenceStatsFontPointSize = 22;
    constexpr qreal kHudTimestampToStatsFontScale = 1.2;

    const qreal shortSide = qMin(stageRect.width(), stageRect.height());
    const qreal hudScale = qMax<qreal>(0.1, shortSide / kHudReferenceShortSide);
    const qreal hudPadding = qMax<qreal>(2.0, kHudReferencePadding * hudScale);
    const int timeFontPointSize = qMax(
        1,
        qRound(static_cast<qreal>(kHudReferenceStatsFontPointSize) * kHudTimestampToStatsFontScale * hudScale)
    );
    const int debugFontPointSize = qMax(1, qRound(static_cast<qreal>(kHudReferenceDebugFontPointSize) * hudScale));
    QFont timestampFont = miacode::preview::scene::previewHudTimestampFontForArea(
        state->hudFontSettings,
        miacode::preview::scene::PreviewHudFontArea::Timestamp,
        timeFontPointSize,
        QFont::DemiBold);
    // Built on demand, NOT alongside timestampFont: the chart-info HUD is off by
    // default, and previewHudTimestampFontForArea() is not free — it runs the
    // per-area custom-family lookup plus a QFontInfo() resolve to validate the
    // fallback. Eagerly constructing this made every HUD repaint pay for a font
    // path the default configuration never draws with.
    const auto chartInfoFont = [&] {
        return miacode::preview::scene::previewHudTimestampFontForArea(
            state->hudFontSettings,
            miacode::preview::scene::PreviewHudFontArea::ChartInfo,
            timeFontPointSize,
            QFont::DemiBold);
    };

    if (state->render.showDebugInfo) {
        appendHudPaintDiag(
            QStringLiteral("branch_enter"),
            [&] { return QStringLiteral("branch=debug state=%1").arg(pointerHex(state)); });
        QFont fpsFont = miacode::preview::scene::previewHudMonoFontForArea(
            state->hudFontSettings,
            miacode::preview::scene::PreviewHudFontArea::DebugInfo,
            debugFontPointSize,
            QFont::Medium);
        const QFontMetrics metrics(fpsFont);
        const qreal leftX = stageRect.left() + hudPadding;
        const qreal baseline0 = stageRect.top() + hudPadding + metrics.ascent();
        const qreal shadowOffset = qMax<qreal>(1.0, 2.0 * hudScale);
        const auto formatMetric = [](double value) {
            return value > 0.0 ? QString::number(value, 'f', 1) : QStringLiteral("na");
        };
        int lineIndex = 0;
        const auto drawDebugLine = [&](const QString& text) {
            const QString tag = QStringLiteral("debug.%1").arg(lineIndex);
            drawHudText(
                painter,
                tag,
                QPointF(leftX, baseline0 + metrics.height() * lineIndex),
                text,
                fpsFont,
                shadowOffset
            );
            ++lineIndex;
        };

        drawDebugLine(
            QStringLiteral("Renderer: %1  API: %2")
                .arg(graphicsInfo.hardwareAccelerated
                    ? QStringLiteral("GPU") : QStringLiteral("CPU"))
                .arg(graphicsInfo.apiName.isEmpty()
                    ? QStringLiteral("Unknown") : graphicsInfo.apiName)
        );
        drawDebugLine(
            QStringLiteral("Device: %1")
                .arg(graphicsInfo.deviceIdentified
                    ? graphicsInfo.deviceName : QStringLiteral("Unknown"))
        );
        // The trailing max=Nms and stut=N metrics surface what an FPS average
        // hides: max is the worst single inter-event interval in the rolling
        // window, stut is the count of intervals exceeding 1.5× the target
        // (i.e. user-noticeable hitches). A clean 60fps line should show
        // max≈17ms stut=0; numbers above that are the actual lag the user
        // perceives even when the FPS figure looks healthy.
        drawDebugLine(
            QStringLiteral("Present: %1 FPS  max=%2ms  stut=%3")
                .arg(QString::number(state->fpsDisplay, 'f', 1))
                .arg(QString::number(state->presentMaxMsDisplay, 'f', 0))
                .arg(state->presentStutterCountDisplay)
        );
        drawDebugLine(
            QStringLiteral("Tick: %1 FPS  max=%2ms  stut=%3")
                .arg(QString::number(state->tickFpsDisplay, 'f', 1))
                .arg(QString::number(state->tickMaxMsDisplay, 'f', 0))
                .arg(state->tickStutterCountDisplay)
        );
        drawDebugLine(
            QStringLiteral("Req: %1 FPS  max=%2ms  stut=%3")
                .arg(QString::number(state->updateRequestFpsDisplay, 'f', 1))
                .arg(QString::number(state->updateRequestMaxMsDisplay, 'f', 0))
                .arg(state->updateRequestStutterCountDisplay)
        );
        drawDebugLine(
            QStringLiteral("Count T/U/P: %1 / %2 / %3")
                .arg(state->tickCount)
                .arg(state->updateRequestCount)
                .arg(state->presentedFrameCount)
        );
        drawDebugLine(
            QStringLiteral("Pacing: %1 %2  Disp: %3")
                .arg(state->framePacingUsesDisplayRefresh ? QStringLiteral("display") : QStringLiteral("fixed"))
                .arg(formatMetric(state->framePacingTargetFps))
                .arg(formatMetric(state->displayRefreshRate))
        );
        if (state->media.presentationMode == miacode::preview::scene::PreviewStageMediaPresentationMode::ExternalQuickMediaItem) {
            QString mediaType = QStringLiteral("none");
            switch (state->media.externalMediaType) {
            case miacode::preview::scene::PreviewExternalStageMediaType::Image:
                mediaType = QStringLiteral("image");
                break;
            case miacode::preview::scene::PreviewExternalStageMediaType::Video:
                mediaType = QStringLiteral("video");
                break;
            case miacode::preview::scene::PreviewExternalStageMediaType::None:
            default:
                break;
            }
            drawHudText(
                painter,
                QStringLiteral("debug.external_media"),
                QPointF(leftX, baseline0 + metrics.height() * lineIndex++),
                QStringLiteral("Media: external/%1").arg(mediaType),
                fpsFont,
                shadowOffset
            );
            drawHudText(
                painter,
                QStringLiteral("debug.external_video"),
                QPointF(leftX, baseline0 + metrics.height() * lineIndex++),
                QStringLiteral("Video: %1  Delta: %2 s")
                    .arg(
                        QStringLiteral("%1 @ %2 FPS")
                            .arg(state->media.externalVideoPlaybackActive ? QStringLiteral("active") : QStringLiteral("idle"))
                            .arg(formatMetric(state->media.externalVideoFrameRate))
                    )
                    .arg(QString::number(state->media.externalClockDeltaSeconds, 'f', 3)),
                fpsFont,
                shadowOffset
            );
            drawHudText(
                painter,
                QStringLiteral("debug.external_video_decode"),
                QPointF(leftX, baseline0 + metrics.height() * lineIndex++),
                QStringLiteral("Decode: %1")
                    .arg(state->media.externalVideoDecodeDesc.trimmed().isEmpty()
                        ? QStringLiteral("None")
                        : state->media.externalVideoDecodeDesc),
                fpsFont,
                shadowOffset
            );
            const QString frameAgeText = state->media.externalVideoFrameAgeMs >= 0
                ? QString::number(state->media.externalVideoFrameAgeMs)
                : QStringLiteral("na");
            drawHudText(
                painter,
                QStringLiteral("debug.external_age"),
                QPointF(leftX, baseline0 + metrics.height() * lineIndex++),
                QStringLiteral("Age: %1 ms  AvgInt: %2  MaxInt: %3")
                    .arg(frameAgeText)
                    .arg(formatMetric(state->media.externalVideoFrameIntervalAvgMs))
                    .arg(formatMetric(state->media.externalVideoFrameIntervalMaxMs)),
                fpsFont,
                shadowOffset
            );
            drawHudText(
                painter,
                QStringLiteral("debug.external_stall"),
                QPointF(leftX, baseline0 + metrics.height() * lineIndex++),
                QStringLiteral("Stall: %1  Count: %2  MediaT: %3 s")
                    .arg(state->media.externalVideoFrameStalled ? QStringLiteral("yes") : QStringLiteral("no"))
                    .arg(state->media.externalVideoFrameStallCount)
                    .arg(QString::number(state->media.externalPlaybackSecond, 'f', 3)),
                fpsFont,
                shadowOffset
            );
        }
    }

    // The HUD honours `hudPlayheadSecondsOverride` when finite so callers
    // can show a timestamp decoupled from the scene time. The full-range
    // video export sets it during the lead-in; the scene itself now plays
    // the real lead-in chart time (it is no longer clamped at chart 0), so
    // this currently matches the scene playhead.
    const double hudPlayheadSeconds =
        miacode::preview::scene::previewFrameStateHudPlayheadSeconds(*state);

    if (state->render.showTimestamp) {
        appendHudPaintDiag(
            QStringLiteral("branch_enter"),
            [&] { return QStringLiteral("branch=timestamp state=%1").arg(pointerHex(state)); });
        const QString timeLabel = miacode::preview::scene::formatPreviewHudTimeLabel(hudPlayheadSeconds);
        const QFontMetrics timeMetrics(timestampFont);
        const QRect timeGlyphs = timeMetrics.boundingRect(timeLabel);
        const qreal timeLeftBearing = qMax(0, -timeGlyphs.left());
        const qreal timeBottomExtent = qMax(timeMetrics.descent(), timeGlyphs.bottom() + 1);
        const qreal timestampShadow = qMax<qreal>(1.0, 2.0 * hudScale);
        drawHudText(
            painter,
            QStringLiteral("timestamp"),
            QPointF(
                stageRect.left() + hudPadding + timeLeftBearing,
                stageRect.bottom() - hudPadding - timeBottomExtent - timestampShadow
            ),
            timeLabel,
            timestampFont,
            qMax<qreal>(1.0, 2.0 * hudScale)
        );
    }

    HudInformationLayoutCache localCache;
    auto& cache = informationLayoutCache ? *informationLayoutCache : localCache;
    const QRectF playfield = miacode::preview::scene::playfieldRectForStage(
        stageRect, state->render.layoutSquareScale);
    const qreal shadowOffset = qMax<qreal>(1.0, 2.0 * hudScale);
    HudInformationLayoutKey key;
    key.stage = stageRect;
    key.playfield = playfield;
    key.timestamp = timestampRegion;
    key.chartTop = debugRegion.isEmpty() ? stageRect.top() + hudPadding : debugRegion.bottom() + hudPadding;
    key.padding = hudPadding;
    key.shadow = shadowOffset;
    key.minimumPointSize = qMax(1, qRound(6 * hudScale));
    key.logicalDpi = painter.device()->logicalDpiY();
    const bool informationHudSupported = stageRect.width() >= qRound(stageRect.height() * 4.0 / 3.0);

    if (state->render.showChartInfoHud && informationHudSupported) {
        key.font = chartInfoFont();
        key.fontPath = state->hudFontSettings.path(miacode::preview::scene::PreviewHudFontArea::ChartInfo);
        key.samples = {state->chartTitle, state->chartArtist, state->chartDifficultyLabel, state->chartDesigner};
        if (!cache.chartKey || *cache.chartKey != key) {
            cache.chart = layoutChartInformation(key, painter);
            cache.chartKey = key;
        }
        const auto& layout = cache.chart;
        for (int i = 0; i < layout.lines.size(); ++i) {
            drawHudText(painter, QStringLiteral("chart_info.line%1").arg(i),
                layout.baselines[i], layout.lines[i], layout.font, shadowOffset);
        }
    }

    if (state->render.showObjectStatsHud && informationHudSupported) {
        const auto& stats = state->hudStatsSnapshot;
        const QString finaleLine = QStringLiteral("%1 %")
            .arg(QString::number(stats.finaleRate, 'f', 2).rightJustified(6, QChar('0')));
        const QString rateLine = QStringLiteral("%1 %")
            .arg(QString::number(stats.deluxeRate, 'f', 4).rightJustified(8, QChar('0')));
        const std::array<int, 6> played = {stats.tapPlayed, stats.holdPlayed, stats.slidePlayed,
            stats.touchPlayed, stats.breakPlayed, stats.combo};
        const std::array<int, 6> totals = {stats.tapTotal, stats.holdTotal, stats.slideTotal,
            stats.touchTotal, stats.breakTotal, stats.totalNotes};
        const std::array<const char*, 6> labels = {"TAP", "HLD", "SLD", "TOH", "BRK", "ALL"};
        QStringList counts;
        QStringList samples = {QStringLiteral("FiNALE Rate:"), QStringLiteral("888.88 %"),
            QStringLiteral("DELUXE Rate:"), QStringLiteral("888.8888 %")};
        for (int i = 0; i < 6; ++i) {
            const QString prefix = QString::fromLatin1(labels[i]) + QStringLiteral(": ");
            counts.append(prefix + QStringLiteral("%1/%2").arg(played[i]).arg(totals[i]));
            const int digits = QString::number(qMax(played[i], totals[i])).size();
            samples.append(prefix + QString(digits, QChar('8')) + QLatin1Char('/') + QString(digits, QChar('8')));
        }
        key.font = miacode::preview::scene::previewHudTimestampFontForArea(
            state->hudFontSettings, miacode::preview::scene::PreviewHudFontArea::ObjectStats,
            qMax(1, qRound(kHudReferenceStatsFontPointSize * hudScale)), QFont::DemiBold);
        key.fontPath = state->hudFontSettings.path(miacode::preview::scene::PreviewHudFontArea::ObjectStats);
        key.samples = samples;
        // Debug text occupies the opposite corner and does not constrain stats.
        key.chartTop = 0;
        if (!cache.statsKey || *cache.statsKey != key) {
            cache.stats = layoutObjectStatistics(key, painter);
            cache.statsKey = key;
        }
        const auto& layout = cache.stats;
        QStringList lines;
        if (layout.compact) {
            lines = {QStringLiteral("FiNALE"), finaleLine, QStringLiteral("DELUXE"), rateLine};
        } else {
            lines = {QStringLiteral("FiNALE Rate:"), finaleLine, QStringLiteral("DELUXE Rate:"), rateLine};
        }
        lines.append(counts);
        for (int i = 0; i < layout.baselines.size(); ++i) {
            drawHudText(painter, QStringLiteral("object_stats.line%1").arg(i),
                layout.baselines[i], lines[i], layout.font, shadowOffset);
        }
    }
}

void paintCenterDisplay(
    QPainter& painter,
    const miacode::preview::scene::PreviewFrameState& state,
    const QSize& canvasSize)
{
    if (state.render.centerDisplayMode == miacode::preview_gameplay::CenterDisplayMode::Off) {
        return;
    }
    const QRectF stageRect = miacode::preview::scene::stageRectForSize(canvasSize);
    // Size/position the center display in the playfield's 1080 logical space, the same
    // design space MajdataPlay's CenterInfoDisplayer lives in, so it tracks the notes ring
    // (and the "Stage Display Scale" / layoutSquareScale) instead of the stage HUD reference.
    const QRectF playRect =
        miacode::preview::scene::playfieldRectForStage(stageRect, state.render.layoutSquareScale);
    const qreal playShort = qMax<qreal>(1.0, qMin(playRect.width(), playRect.height()));
    const miacode::preview::scene::PreviewHudStats stats = state.hudStatsSnapshot;
    const QString title = centerDisplayTitle(state.render.centerDisplayMode);
    const QString value = centerDisplayValue(state.render.centerDisplayMode, stats);
    if (title.isEmpty() || value.isEmpty()) {
        return;
    }
    // MajdataPlay reference (1080 design space): value TMP fontSize 65, header 44,
    // header centered below the value (which is centered on the playfield centre).
    // MajdataPlay puts the header 137.3px below; we pull it up to 2/3 that gap (~91.5px).
    const int valuePixelSize = qMax(1, qRound(65.0 / 1080.0 * playShort));
    const int titlePixelSize = qMax(1, qRound(44.0 / 1080.0 * playShort));
    const qreal headerCenterOffsetY = (137.3 * 2.0 / 3.0) / 1080.0 * playShort;

    QFont titleFont = miacode::preview::scene::previewHudTimestampFontForArea(
        state.hudFontSettings,
        miacode::preview::scene::PreviewHudFontArea::CenterDisplay,
        titlePixelSize,
        QFont::Black);
    titleFont.setPixelSize(titlePixelSize);
    QFont valueFont = miacode::preview::scene::previewHudTimestampFontForArea(
        state.hudFontSettings,
        miacode::preview::scene::PreviewHudFontArea::CenterDisplay,
        valuePixelSize,
        QFont::Black);
    valueFont.setPixelSize(valuePixelSize);
    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF valueMetrics(valueFont);

    const QPointF center = playRect.center();
    // Value glyph visually centered on the playfield centre.
    const qreal valueBaselineY = center.y() + (valueMetrics.ascent() - valueMetrics.descent()) / 2.0;
    const QPointF valueBaseline(
        center.x() - valueMetrics.horizontalAdvance(value) / 2.0,
        valueBaselineY);
    // Header label centered horizontally, sitting below the value.
    const qreal headerCenterY = center.y() + headerCenterOffsetY;
    const qreal titleBaselineY = headerCenterY + (titleMetrics.ascent() - titleMetrics.descent()) / 2.0;
    const QPointF titleBaseline(
        center.x() - titleMetrics.horizontalAdvance(title) / 2.0,
        titleBaselineY);

    const QColor valueColor = centerDisplayValueColor(state.render.centerDisplayMode, stats);
    const QColor titleColor = centerDisplayHeaderColor(state.render.centerDisplayMode);
    // Drop-shadow offset matches the CenterInfoDisplayer TMP underlay direction
    // (_UnderlayOffsetX 0.59, _UnderlayOffsetY -0.86; +x right, -y up in TMP =>
    // down-right on screen). Scaled per glyph size since the underlay lives in the
    // font's local space (so the larger value glyph casts a proportionally larger shadow).
    const QPointF titleShadow(titlePixelSize * 0.035, titlePixelSize * 0.051);
    const QPointF valueShadow(valuePixelSize * 0.035, valuePixelSize * 0.051);
    drawOutlinedHudText(painter, titleBaseline, title, titleFont,
        qMax<qreal>(1.5, titlePixelSize * 0.05), titleShadow, titleColor);
    drawOutlinedHudText(painter, valueBaseline, value, valueFont,
        qMax<qreal>(2.0, valuePixelSize * 0.05), valueShadow, valueColor);
}

}  // namespace miacode::preview::hud
