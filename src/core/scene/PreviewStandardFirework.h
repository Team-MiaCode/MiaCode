#pragma once

#include <QByteArray>
#include <QPointF>
#include <QVector>
#include <array>
#include <memory>

namespace miacode::preview::scene {

enum class PreviewStandardFireworkMaterial {
    Composite,
    Star,
    Rays,
    Ring,
    Rainbow,
};

struct PreviewStandardFireworkSprite {
    PreviewStandardFireworkMaterial material = PreviewStandardFireworkMaterial::Star;
    QPointF offset;
    double diameter = 0.0;
    double rotation = 0.0;
    float exposure = 0.0f;
    std::array<float, 4> color{};
};

class PreviewStandardFireworkBurst
{
public:
    QVector<PreviewStandardFireworkSprite> sample(double elapsedSeconds) const;

private:
    friend class PreviewStandardFireworkEffect;
    struct Data;
    std::shared_ptr<const Data> data_;
};

class PreviewStandardFireworkEffect
{
public:
    static std::shared_ptr<const PreviewStandardFireworkEffect> fromJson(const QByteArray& json);
    PreviewStandardFireworkBurst createBurst(quint32 seed) const;

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

}  // namespace miacode::preview::scene
