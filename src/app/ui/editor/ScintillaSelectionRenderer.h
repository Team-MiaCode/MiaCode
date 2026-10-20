#pragma once

#include <QColor>
#include <QVector>
#include <QRectF>
class QQuickWindow;
class QSGNode;

namespace miacode::ui {
// Each captured range forms one filled contour beneath the text.
void synchronizeScintillaHighlight(QSGNode*& node, QSGNode* parent, QQuickWindow* window,
                                   const QVector<QRectF>& rectangles, const QColor& color);
}
