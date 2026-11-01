#include "core/scene/PreviewStandardFirework.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
#include <random>

namespace miacode::preview::scene {
namespace {

constexpr double kVelocitySampleRate = 120.0;
constexpr double kFullTurn = 6.283185307179586;

struct ScalarCurveKey {
    double time;
    double value;
    double inTangent;
    double outTangent;
};

QVector<ScalarCurveKey> readCurveKeys(const QJsonArray& array)
{
    QVector<ScalarCurveKey> result;
    for (const auto& value : array) {
        const auto key = value.toArray();
        result.append({key[0].toDouble(), key[1].toDouble(), key[2].toDouble(), key[3].toDouble()});
    }
    return result;
}

double sampleHermiteCurve(const QVector<ScalarCurveKey>& keys, double time)
{
    if (keys.isEmpty()) return 1.0;
    if (time <= keys.front().time) return keys.front().value;
    for (qsizetype index = 1; index < keys.size(); ++index) {
        const auto& next = keys[index];
        if (time > next.time) continue;
        const auto& previous = keys[index - 1];
        const double duration = next.time - previous.time;
        const double u = (time - previous.time) / duration;
        const double u2 = u * u;
        const double u3 = u2 * u;
        return (2 * u3 - 3 * u2 + 1) * previous.value
            + (u3 - 2 * u2 + u) * duration * previous.outTangent
            + (-2 * u3 + 3 * u2) * next.value
            + (u3 - u2) * duration * next.inTangent;
    }
    return keys.back().value;
}

struct ParticleCurve {
    int mode = 0;
    double maximum = 0.0;
    double minimum = 0.0;
    QVector<ScalarCurveKey> high;
    QVector<ScalarCurveKey> low;

    explicit ParticleCurve(const QJsonValue& value = {})
    {
        const auto object = value.toObject();
        mode = object["mode"].toInt();
        maximum = object["max"].toDouble();
        minimum = object["min"].toDouble();
        high = readCurveKeys(object["hi"].toArray());
        low = readCurveKeys(object["lo"].toArray());
    }

    double sample(double time, double random) const
    {
        switch (mode) {
        case 1: return sampleHermiteCurve(high, time) * maximum;
        case 2: return std::lerp(sampleHermiteCurve(low, time) * minimum, sampleHermiteCurve(high, time) * maximum, random);
        case 3: return std::lerp(minimum, maximum, random);
        default: return maximum;
        }
    }
};

struct GradientKey {
    double time;
    std::array<float, 4> value{};
};

QVector<GradientKey> readGradientKeys(const QJsonArray& array, int components)
{
    QVector<GradientKey> result;
    for (const auto& entry : array) {
        const auto key = entry.toArray();
        GradientKey parsed{key[0].toDouble()};
        for (int component = 0; component < components; ++component) {
            parsed.value[component] = key[component + 1].toDouble();
        }
        result.append(parsed);
    }
    return result;
}

std::array<float, 4> sampleGradientKeys(const QVector<GradientKey>& keys, double time)
{
    if (time <= keys.front().time) return keys.front().value;
    for (qsizetype index = 1; index < keys.size(); ++index) {
        if (time > keys[index].time) continue;
        const auto& previous = keys[index - 1];
        const auto& next = keys[index];
        const float u = (time - previous.time) / (next.time - previous.time);
        std::array<float, 4> result{};
        for (int component = 0; component < 4; ++component) {
            result[component] = std::lerp(previous.value[component], next.value[component], u);
        }
        return result;
    }
    return keys.back().value;
}

struct ParticleGradient {
    QVector<GradientKey> rgb;
    QVector<GradientKey> alpha;

    explicit ParticleGradient(const QJsonValue& value)
    {
        const auto object = value.toObject();
        rgb = readGradientKeys(object["rgb"].toArray(), 3);
        alpha = readGradientKeys(object["alpha"].toArray(), 1);
    }

    std::array<float, 4> sample(double time) const
    {
        auto color = sampleGradientKeys(rgb, time);
        color[3] = sampleGradientKeys(alpha, time)[0];
        return color;
    }
};

PreviewStandardFireworkMaterial materialFromName(const QString& name)
{
    if (name == QLatin1String("rainbow")) return PreviewStandardFireworkMaterial::Rainbow;
    if (name == QLatin1String("ring")) return PreviewStandardFireworkMaterial::Ring;
    if (name == QLatin1String("rays")) return PreviewStandardFireworkMaterial::Rays;
    return PreviewStandardFireworkMaterial::Star;
}

struct FireworkEmitter {
    PreviewStandardFireworkMaterial material;
    double delay;
    double duration;
    double rate;
    QVector<QPointF> bursts;
    ParticleCurve life, size, speed, rotation, angularVelocity, sizeCurve, velocityLimit;
    ParticleGradient colorHigh, colorLow;
    bool colorRandom;
    bool limitVelocity;
    double radius;
    double radiusThickness;
    float exposure;

    explicit FireworkEmitter(const QJsonObject& object)
        : material(materialFromName(object["material"].toString())), delay(object["delay"].toDouble()),
          duration(object["duration"].toDouble()), rate(object["rate"].toDouble()),
          life(object["life"]), size(object["size"]), speed(object["speed"]),
          rotation(object["rotation"]), angularVelocity(object["angularVelocity"]),
          sizeCurve(object["sizeCurve"]), velocityLimit(object["velocityLimit"]),
          colorHigh(object["colorHi"]), colorLow(object["colorLo"]),
          colorRandom(object["colorRandom"].toBool()), limitVelocity(object["velocityLimit"].isObject()),
          radius(object["radius"].toDouble()), radiusThickness(object["radiusThickness"].toDouble()),
          exposure(object["exposure"].toDouble())
    {
        for (const auto& value : object["bursts"].toArray()) {
            const auto burst = value.toArray();
            bursts.append(QPointF(burst[0].toDouble(), burst[1].toDouble()));
        }
    }
};

struct FireworkParticle {
    qsizetype emitter;
    double birth;
    double life;
    double diameter;
    double angle;
    double rotation;
    double spin;
    double random;
    QVector<double> distances;
};

}  // namespace

struct PreviewStandardFireworkEffect::Data {
    QVector<FireworkEmitter> emitters;
};

struct PreviewStandardFireworkBurst::Data {
    QVector<FireworkEmitter> emitters;
    QVector<FireworkParticle> particles;
};

std::shared_ptr<const PreviewStandardFireworkEffect> PreviewStandardFireworkEffect::fromJson(const QByteArray& json)
{
    const auto array = QJsonDocument::fromJson(json).array();
    if (array.isEmpty()) return {};
    auto data = std::make_shared<Data>();
    for (const auto& value : array) data->emitters.append(FireworkEmitter(value.toObject()));
    auto effect = std::make_shared<PreviewStandardFireworkEffect>();
    effect->data_ = std::move(data);
    return effect;
}

PreviewStandardFireworkBurst PreviewStandardFireworkEffect::createBurst(quint32 seed) const
{
    auto data = std::make_shared<PreviewStandardFireworkBurst::Data>();
    data->emitters = data_->emitters;
    std::mt19937 generator(seed);
    const auto random = [&generator] { return double(generator()) / 4294967296.0; };
    for (qsizetype index = 0; index < data->emitters.size(); ++index) {
        const auto& emitter = data->emitters[index];
        QVector<double> births;
        for (const auto& burst : emitter.bursts) {
            for (int count = 0; count < int(burst.y()); ++count) births.append(burst.x());
        }
        for (int count = 0; count < qRound(emitter.rate * emitter.duration); ++count) {
            births.append(count / emitter.rate);
        }
        for (double birth : births) {
            FireworkParticle particle{};
            particle.emitter = index;
            particle.birth = birth + emitter.delay;
            particle.random = random();
            particle.life = emitter.life.sample(0.0, particle.random);
            particle.diameter = emitter.size.sample(0.0, random()) * 100.0;
            particle.angle = random() * kFullTurn;
            particle.rotation = emitter.rotation.sample(0.0, random());
            const double radius = emitter.radius * (1.0 - emitter.radiusThickness * random()) * 100.0;
            const double speed = emitter.speed.sample(0.0, particle.random) * 100.0;
            particle.spin = emitter.angularVelocity.sample(0.0, random());
            const int steps = int(std::ceil(particle.life * kVelocitySampleRate));
            particle.distances.reserve(steps + 1);
            particle.distances.append(radius);
            // Precompute the integral once. Seeking and export sample the same trajectory.
            for (int step = 0; step < steps; ++step) {
                const double time = (step + 0.5) / kVelocitySampleRate;
                const double limit = emitter.limitVelocity
                    ? emitter.velocityLimit.sample(time / particle.life, particle.random) * 100.0 : speed;
                particle.distances.append(particle.distances.back()
                    + std::min(speed, std::max(0.0, limit)) / kVelocitySampleRate);
            }
            data->particles.append(std::move(particle));
        }
    }
    PreviewStandardFireworkBurst burst;
    burst.data_ = std::move(data);
    return burst;
}

QVector<PreviewStandardFireworkSprite> PreviewStandardFireworkBurst::sample(double elapsedSeconds) const
{
    QVector<PreviewStandardFireworkSprite> sprites;
    if (!data_) return sprites;
    sprites.reserve(data_->particles.size());
    for (const auto& particle : data_->particles) {
        const double age = elapsedSeconds - particle.birth;
        if (age < 0.0 || age >= particle.life) continue;
        const auto& emitter = data_->emitters[particle.emitter];
        const double time = age / particle.life;
        const double size = emitter.sizeCurve.sample(time, particle.random);
        auto color = emitter.colorHigh.sample(time);
        if (emitter.colorRandom) {
            const auto low = emitter.colorLow.sample(time);
            for (int component = 0; component < 4; ++component) {
                color[component] = std::lerp(low[component], color[component], float(particle.random));
            }
        }
        if (size <= 0.0 || color[3] <= 0.0f) continue;
        const double distancePosition = age * kVelocitySampleRate;
        const int step = int(distancePosition);
        const double distance = std::lerp(particle.distances[step], particle.distances[step + 1], distancePosition - step);
        sprites.append({emitter.material,
            QPointF(std::cos(particle.angle) * distance, -std::sin(particle.angle) * distance),
            particle.diameter * size, particle.rotation + particle.spin * age, emitter.exposure, color});
    }
    return sprites;
}

}  // namespace miacode::preview::scene
