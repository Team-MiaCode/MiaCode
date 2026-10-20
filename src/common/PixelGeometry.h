#pragma once

#include <QSize>
#include <QSizeF>
#include <QtMath>

namespace miacode::pixel_geometry {

inline QSize coveringSize(const QSizeF& size)
{
    return QSize(qMax(1, qCeil(size.width())), qMax(1, qCeil(size.height())));
}

} // namespace miacode::pixel_geometry
