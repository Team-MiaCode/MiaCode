#include "app/ui/editor/ScintillaSelectionRenderer.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGTransformNode>
#include <algorithm>
#include <cmath>

namespace miacode::ui {
namespace {
void appendPoint(QVector<QPointF>& edge, const QPointF& point)
{
    if (!edge.isEmpty() && edge.back() == point) return;
    if (edge.size() > 1) {
        const auto before = edge.at(edge.size() - 2);
        const auto last = edge.back();
        if ((before.x() == last.x() && last.x() == point.x())
            || (before.y() == last.y() && last.y() == point.y())) {
            edge.back() = point;
            return;
        }
    }
    edge.append(point);
}

QPainterPath selectionOutline(const QVector<QRectF>& rows)
{
    QPainterPath path;
    QVector<QPointF> left, right;
    auto closeContour = [&] {
        if (left.isEmpty()) return;
        QVector<QPointF> points = right;
        for (auto it = left.crbegin(); it != left.crend(); ++it) points.append(*it);
        for (int i = 0; i < points.size(); ++i) {
            const auto previous = points.at((i + points.size() - 1) % points.size());
            const auto current = points.at(i);
            const auto next = points.at((i + 1) % points.size());
            const qreal incoming = std::hypot(current.x() - previous.x(), current.y() - previous.y());
            const qreal outgoing = std::hypot(next.x() - current.x(), next.y() - current.y());
            const qreal radius = qMin(3.0, qMin(incoming / 2, outgoing / 2));
            const auto start = current - (current - previous) * (radius / incoming);
            const auto end = current + (next - current) * (radius / outgoing);
            if (i == 0) path.moveTo(start);
            else path.lineTo(start);
            // The same contour rounds convex outside and concave row joins.
            constexpr qreal quarterCircleControl = 0.5522847498307936;
            path.cubicTo(start + (current - start) * quarterCircleControl,
                         end + (current - end) * quarterCircleControl, end);
        }
        path.closeSubpath();
        left.clear();
        right.clear();
    };
    QRectF previous;
    for (const auto& row : rows) {
        if (!previous.isEmpty()) {
            const qreal overlapLeft = qMax(previous.left(), row.left());
            const qreal overlapRight = qMin(previous.right(), row.right());
            if (std::abs(row.top() - previous.bottom()) <= qMax(0.5, row.height() / 8)
                && overlapRight - overlapLeft > qMax(0.5, row.height() / 8)) {
                appendPoint(left, {overlapLeft, previous.bottom()});
                appendPoint(left, {overlapLeft, row.top()});
                appendPoint(right, {overlapRight, previous.bottom()});
                appendPoint(right, {overlapRight, row.top()});
            } else closeContour();
        }
        appendPoint(left, row.topLeft());
        appendPoint(left, row.bottomLeft());
        appendPoint(right, row.topRight());
        appendPoint(right, row.bottomRight());
        previous = row;
    }
    closeContour();
    return path;
}

class RoundedSelectionNode final : public QSGTransformNode
{
public:
    void synchronize(QQuickWindow* window, const QVector<QRectF>& rectangles, const QColor& color)
    {
        const QPointF origin = rectangles.isEmpty() ? QPointF{} : rectangles.front().topLeft();
        if (origin != origin_) {
            origin_ = origin;
            QMatrix4x4 translation;
            translation.translate(origin.x(), origin.y());
            setMatrix(translation);
        }
        QVector<QRectF> localRectangles;
        localRectangles.reserve(rectangles.size());
        for (const auto& rectangle : rectangles) localRectangles.append(rectangle.translated(-origin));
        const qreal dpr = window->effectiveDevicePixelRatio();
        if (localRectangles == rectangles_ && color == color_ && dpr == dpr_) return;
        rectangles_ = localRectangles;
        color_ = color;
        dpr_ = dpr;
        if (localRectangles.isEmpty()) {
            if (image_) {
                removeChildNode(image_);
                delete image_;
                image_ = nullptr;
            }
            return;
        }
        std::sort(localRectangles.begin(), localRectangles.end(), [](const QRectF& a, const QRectF& b) {
            return a.top() == b.top() ? a.left() < b.left() : a.top() < b.top();
        });
        QVector<QRectF> rows;
        for (const auto& rectangle : std::as_const(localRectangles)) {
            if (!rows.isEmpty() && rows.back().top() == rectangle.top() && rows.back().height() == rectangle.height()
                && rectangle.left() <= rows.back().right()) rows.back() = rows.back().united(rectangle);
            else rows.append(rectangle);
        }
        const QPainterPath outline = selectionOutline(rows);
        const QRectF bounds = outline.boundingRect().adjusted(-1, -1, 1, 1);
        QImage image(QSize(qCeil(bounds.width() * dpr), qCeil(bounds.height() * dpr)), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(-bounds.topLeft());
        painter.fillPath(outline, color);
        painter.end();
        const bool createImageNode = !image_;
        if (createImageNode) {
            image_ = window->createImageNode();
            image_->setOwnsTexture(true);
            image_->setFiltering(QSGTexture::Linear);
        }
        image_->setTexture(window->createTextureFromImage(image));
        image_->setRect(bounds);
        if (createImageNode) appendChildNode(image_);
    }
private:
    QSGImageNode* image_ = nullptr;
    QPointF origin_;
    QVector<QRectF> rectangles_;
    QColor color_;
    qreal dpr_ = 0;
};
}

void synchronizeScintillaHighlight(QSGNode*& node, QSGNode* parent, QQuickWindow* window,
                                   const QVector<QRectF>& rectangles, const QColor& color)
{
    if (!node) {
        node = new RoundedSelectionNode;
        parent->appendChildNode(node);
    }
    static_cast<RoundedSelectionNode*>(node)->synchronize(window, rectangles, color);
}
}
