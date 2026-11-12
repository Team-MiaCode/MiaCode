#pragma once
#include <QImage>
#include <QVector>
#include <QtMath>

namespace miacode::preview::scene {
inline int selectionOutlinePadding(qreal width)
{
    return qCeil(qMax(0.5, width) * 1.75 + 0.5);
}

// A one-pixel contour with a small gap, rather than a filled alpha dilation.
// Only background connected to the outside is decorated: tap/hold holes stay
// clear. Disabled edges repeat the seam texel so sliced holds join seamlessly.
// Width is in image pixels; callers use DPR to keep it independent of note size.
inline QImage selectionOutlineImage(const QImage& source, qreal width,
    bool left = true, bool right = true, bool top = true, bool bottom = true)
{
    if (source.isNull()) return {};
    width = qMax(0.5, width);
    const qreal gap = width * 0.75;
    const int radius = selectionOutlinePadding(width);
    const QImage input = source.convertToFormat(QImage::Format_ARGB32);
    const int padL = left ? radius : 0, padR = right ? radius : 0;
    const int padT = top ? radius : 0, padB = bottom ? radius : 0;
    QImage output(input.width() + padL + padR, input.height() + padT + padB,
        QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);
    const auto alpha = [&](int x, int y) {
        if ((left && x < 0) || (right && x >= input.width())
            || (top && y < 0) || (bottom && y >= input.height())) return 0;
        return qAlpha(input.pixel(qBound(0, x, input.width()-1), qBound(0, y, input.height()-1)));
    };

    // Flood only the transparent background. Alpha at half coverage defines the
    // silhouette; the original alpha still supplies the antialiased contour.
    QVector<quint8> exterior(output.width() * output.height(), 0);
    QVector<int> pending;
    const auto visit = [&](int x, int y) {
        if (x < 0 || x >= output.width() || y < 0 || y >= output.height()) return;
        const int index = y * output.width() + x;
        if (exterior[index] || alpha(x-padL, y-padT) >= 128) return;
        exterior[index] = 1;
        pending.append(index);
    };
    for (int x = 0; x < output.width(); ++x) {
        if (top) visit(x, 0);
        if (bottom) visit(x, output.height()-1);
    }
    for (int y = 0; y < output.height(); ++y) {
        if (left) visit(0, y);
        if (right) visit(output.width()-1, y);
    }
    for (qsizetype i = 0; i < pending.size(); ++i) {
        const int x = pending[i] % output.width(), y = pending[i] / output.width();
        visit(x-1, y); visit(x+1, y); visit(x, y-1); visit(x, y+1);
    }

    struct Sample { int x, y; qreal outer, inner; };
    QVector<Sample> samples;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const qreal distance = qSqrt(qreal(dx*dx + dy*dy));
            const qreal outer = qBound(0.0, gap + width + 0.5 - distance, 1.0);
            if (outer > 0.0)
                samples.append({dx, dy, outer, qBound(0.0, gap + 0.5 - distance, 1.0)});
        }
    }
    for (int y = 0; y < output.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(output.scanLine(y));
        for (int x = 0; x < output.width(); ++x) {
            const int sx = x-padL, sy = y-padT;
            if (!exterior[y*output.width()+x]) continue;
            qreal outer = 0.0, inner = 0.0;
            for (const auto& sample : samples) {
                const int value = alpha(sx+sample.x, sy+sample.y);
                outer = qMax(outer, value * sample.outer);
                inner = qMax(inner, value * sample.inner);
            }
            const int a = qRound(qMax(0.0, outer-inner) * 0.8);
            row[x] = qRgba(a, (220*a)/255, (80*a)/255, a);
        }
    }
    output.setDevicePixelRatio(source.devicePixelRatio());
    return output;
}
}
