#include "preview/quick_scene/PreviewQuickJudgeFireworkLayer.h"
#include "preview/quick_scene/PreviewStandardFireworkNode.h"

#include "preview/quick_scene/PreviewTextureRepository.h"
#include "preview/quick_scene/PreviewModernFireworkPostProcess.h"
#include "core/scene/PreviewJudgeFireworkLayerState.h"
#include "core/scene/PreviewPreparedSceneCache.h"
#include "core/scene/PreviewSceneGeometry.h"
#include "core/video/PreviewVideoGeometryConfig.h"

#include <QMatrix4x4>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QSGClipNode>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGNode>
#include <QSGTexture>
#include <QtMath>

#include <array>
#include <cmath>
#include <cstring>
#include <functional>

namespace {

constexpr int kModernFireworkGlowFieldCount = 11;
struct ModernFireworkGlowField {
    float axisX, axisY, inverseReach, inverseWidth;
    float centerX, centerY, weight, padding;
};
constexpr int kModernFireworkUniformBufferSize = 192 + kModernFireworkGlowFieldCount * sizeof(ModernFireworkGlowField);
constexpr qreal kModernFireworkQuadRadiusMin = 1.0;
constexpr int kModernFireworkClipSegments = 64;
constexpr int kModernFireworkRenderFlagDrawStripe = 0x1;
constexpr int kModernFireworkRenderFlagDrawBigBall = 0x2;
constexpr int kModernFireworkRenderFlagDrawSmallBall = 0x4;
constexpr int kModernFireworkRenderFlagUseTexture = 0x8;
constexpr int kModernFireworkRenderFlagDrawFallbackBall = 0x10;
constexpr int kModernFireworkRenderFlagWarmup = 0x20;

float modernFireworkStarSeed(qreal triggerSecond)
{
    QRandomGenerator random(static_cast<quint32>(qHash(triggerSecond, size_t{0})));
    return static_cast<float>(random.generateDouble() * 4096.0);
}

float modernFireworkSmoothStep(float value)
{
    const float bounded = qBound(0.0f, value, 1.0f);
    return bounded * bounded * (3.0f - 2.0f * bounded);
}

std::array<ModernFireworkStarState, kModernFireworkStarCount> modernFireworkStars(
    float clipTime, float clipRadius, float angleSeed)
{
    constexpr std::array<std::array<float, 3>, kModernFireworkStarCount> positions = {{
        {290,130,19},{558,178,13},{570,199,12},{617,285,21},
        {468,268,12},{286,241,8},{258,288,21},{241,322,12},
        {244,382,20},{237,397,14},{259,441,14},{511,362,8},
        {514,377,7},{486,402,22},{489,423,12},{477,441,14},
        {459,457,13},{420,476,23},{404,487,12},{598,475,14},
        {585,503,15},{539,535,23},{526,563,10},{468,607,8},
        {184,490,20},{155,535,9},{218,557,13},{149,287,19},
        {187,225,8},{220,183,13},{95,398,10},{333,78,9}
    }};
    constexpr float lateDelays[8] = {.39f,.53f,.28f,.34f,.61f,.46f,.65f,.58f};
    std::array<ModernFireworkStarState, kModernFireworkStarCount> stars{};
    const float rotation = angleSeed * 6.28318530718f / 4096.0f;
    const float cosAngle = std::cos(rotation), sinAngle = std::sin(rotation);
    for (int i = 0; i < kModernFireworkStarCount; ++i) {
        const auto& point = positions[i];
        const float variation = static_cast<float>((i * 17 + 7) % 29) / 28.0f;
        const float delay = i >= 24 ? lateDelays[i - 24]
            : (point[2] >= 18 ? .068f : (i == 5 || i == 11) ? .085f : .145f) + variation * .065f;
        const float age = clipTime - delay;
        const float x = (point[0] - 360.0f) / 350.0f;
        const float y = (point[1] - 360.0f) / 350.0f;
        const float travelLifetime = (.97f / std::hypot(x, y) - 1.0f) / .55f;
        const float desiredStart = .10f + variation * .08f;
        const float desiredDuration = .28f + static_cast<float>((i * 11 + 3) % 23) / 22.0f * .16f;
        const float lifetime = qMin(qMin(desiredStart + desiredDuration, travelLifetime), 1.0f - delay);
        const float shrinkStart = desiredStart * lifetime / (desiredStart + desiredDuration);
        const float departure = qBound(0.0f, (age - shrinkStart) / (lifetime - shrinkStart), 1.0f);
        if (age < 0.0f || departure >= 1.0f) {
            continue;
        }
        const float distance = clipRadius * (1.0f + .55f * age);
        const float width = .66f + static_cast<float>((i % 12 + (i / 12) * 2) % 5) * .035f;
        stars[i] = {(x * cosAngle - y * sinAngle) * distance,
            (x * sinAngle + y * cosAngle) * distance,
            point[2] / 350.0f * clipRadius * (1.0f - modernFireworkSmoothStep(departure)) / width, 1.0f};
    }
    return stars;
}

std::array<ModernFireworkGlowField, kModernFireworkGlowFieldCount> modernFireworkGlowFields(float time)
{
    constexpr float directions[] = {0.0f, .39f, 1.05f, 1.42f, 2.19f, 2.64f, 3.02f, 3.85f, 4.21f, 4.90f, 5.75f};
    struct Shape { float speed, variation, width; };
    static const auto shapes = [] {
        std::array<Shape, kModernFireworkGlowFieldCount> values{};
        for (int i = 0; i < kModernFireworkGlowFieldCount; ++i) {
            values[i] = {5.0f + 3.0f * (.5f + .5f * std::sin(i * 1.73f)),
                .5f + .5f * std::sin(i * 4.13f + 1.7f),
                .023f + .006f * (.5f + .5f * std::cos(i * 2.91f))};
        }
        return values;
    }();
    const float lightRetirement = qBound(0.0f, (time + 1.0f / 30.0f - .567f) / .433f, 1.0f);
    const float dispersion = modernFireworkSmoothStep((time + 1.0f / 30.0f) / .050f)
        * (1.0f - std::pow(lightRetirement, 4.0f));
    std::array<ModernFireworkGlowField, kModernFireworkGlowFieldCount> fields{};
    for (int i = 0; i < kModernFireworkGlowFieldCount; ++i) {
        const auto& shape = shapes[i];
        const float phase = time * shape.speed;
        const float angle = directions[i] + qDegreesToRadians(time * 70.0f)
            + .075f * std::sin(phase + i * 2.61f) * dispersion;
        const float x = std::cos(angle), y = std::sin(angle);
        const float reach = (.126f + (.207f - .126f) * shape.variation)
            * (1.0f + .16f * std::sin(phase + i * 3.17f) * dispersion);
        const float width = shape.width * (1.0f + .08f * std::sin(phase * .83f + i * 1.91f) * dispersion);
        const float center = (.010f + .006f * shape.variation) * dispersion;
        fields[i] = {x, y, 1.0f / (.026f + (reach - .026f) * dispersion),
            1.0f / (.026f + (width - .026f) * dispersion), x * center, y * center,
            .88f + .12f * shape.variation, 0.0f};
    }
    return fields;
}

class ModernFireworkMaterial;

struct ModernFireworkVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

const QSGGeometry::AttributeSet& modernFireworkVertexAttributes()
{
    static QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
    };
    static QSGGeometry::AttributeSet attributeSet = {
        2,
        static_cast<int>(sizeof(ModernFireworkVertex)),
        attributes
    };
    return attributeSet;
}

QImage& fallbackModernFireworkTextureImage()
{
    static QImage image = []() {
        QImage img(1, 1, QImage::Format_RGBA8888_Premultiplied);
        img.fill(Qt::white);
        return img;
    }();
    return image;
}

QImage& modernFireworkBrightBlockImage()
{
    static QImage image(QStringLiteral(
        ":/preview/fireworks/modern/bright_block.png"
    ));
    return image;
}

class ModernFireworkClipNode final : public QSGClipNode
{
public:
    ModernFireworkClipNode()
    {
        geometry_ = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), kModernFireworkClipSegments * 3);
        geometry_->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry_->setVertexDataPattern(QSGGeometry::DynamicPattern);
        setGeometry(geometry_);
        setFlag(QSGNode::OwnsGeometry, true);
        setIsRectangular(false);
    }

    void updateClip(const QPointF& center, qreal radius)
    {
        if (center == center_ && radius == radius_) return;
        center_ = center;
        radius_ = radius;
        auto* vertices = geometry_->vertexDataAsPoint2D();
        for (int segmentIndex = 0; segmentIndex < kModernFireworkClipSegments; ++segmentIndex) {
            const qreal t0 = static_cast<qreal>(segmentIndex) / static_cast<qreal>(kModernFireworkClipSegments);
            const qreal t1 = static_cast<qreal>(segmentIndex + 1) / static_cast<qreal>(kModernFireworkClipSegments);
            const qreal angle0 = -t0 * 360.0;
            const qreal angle1 = -t1 * 360.0;
            const QPointF p0(
                center.x() + radius * qCos(qDegreesToRadians(angle0)),
                center.y() + radius * qSin(qDegreesToRadians(angle0))
            );
            const QPointF p1(
                center.x() + radius * qCos(qDegreesToRadians(angle1)),
                center.y() + radius * qSin(qDegreesToRadians(angle1))
            );
            const int vertexIndex = segmentIndex * 3;
            vertices[vertexIndex + 0].set(static_cast<float>(center.x()), static_cast<float>(center.y()));
            vertices[vertexIndex + 1].set(static_cast<float>(p0.x()), static_cast<float>(p0.y()));
            vertices[vertexIndex + 2].set(static_cast<float>(p1.x()), static_cast<float>(p1.y()));
        }
        geometry_->markVertexDataDirty();
        setClipRect(QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0));
        markDirty(QSGNode::DirtyGeometry);
    }

private:
    QPointF center_;
    qreal radius_ = -1.0;
    QSGGeometry* geometry_ = nullptr;
};

class ModernFireworkShader final : public QSGMaterialShader
{
public:
    ModernFireworkShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/src/preview/quick_scene/shaders/PreviewModernFireworkMaterial.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/src/preview/quick_scene/shaders/PreviewModernFireworkMaterial.frag.qsb"));
        setFlag(UpdatesGraphicsPipelineState, true);
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override;
    void updateSampledImage(
        RenderState& state,
        int binding,
        QSGTexture** texture,
        QSGMaterial* newMaterial,
        QSGMaterial* oldMaterial
    ) override;
    bool updateGraphicsPipelineState(
        RenderState& state,
        GraphicsPipelineState* ps,
        QSGMaterial* newMaterial,
        QSGMaterial* oldMaterial
    ) override;
};

class ModernFireworkMaterial final : public QSGMaterial
{
public:
    ModernFireworkMaterial()
    {
        setFlag(Blending, true);
        setFlag(RequiresFullMatrix, true);
        setFlag(NoBatching, true);
    }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType materialType;
        return &materialType;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override
    {
        return new ModernFireworkShader();
    }

    int compare(const QSGMaterial* other) const override
    {
        if (this == other) return 0;
        return std::less<const QSGMaterial*>{}(this, other) ? -1 : 1;
    }

    void setState(
        QSGTexture* texture,
        QSGTexture* brightBlockTexture,
        QSGTexture* bigBallTexture,
        bool useTexture,
        float quadRadius,
        float clipRadius,
        float outerRadius,
        float fireworkAlpha,
        float fireworkBrightness,
        float fireworkRotationDegrees,
        float holeRadius,
        float holeMaskRadius,
        float colorBallRadius,
        float colorBallBigRadius,
        float colorBallAlpha,
        float colorBallBigAlpha,
        float fallbackColorBallRadius,
        float fallbackColorBallAlpha,
        float renderFlags,
        float sourceAspect,
        float clipTimeSeconds,
        float life01,
        float starAngleSeed,
        const std::array<ModernFireworkGlowField, kModernFireworkGlowFieldCount>& glowFields,
        const QRectF& sourceRectNormalized
    )
    {
        texture_ = texture;
        brightBlockTexture_ = brightBlockTexture;
        bigBallTexture_ = bigBallTexture;
        useTexture_ = useTexture;
        quadRadius_ = quadRadius;
        clipRadius_ = clipRadius;
        outerRadius_ = outerRadius;
        fireworkAlpha_ = fireworkAlpha;
        fireworkBrightness_ = fireworkBrightness;
        fireworkRotationDegrees_ = fireworkRotationDegrees;
        holeRadius_ = holeRadius;
        holeMaskRadius_ = holeMaskRadius;
        colorBallRadius_ = colorBallRadius;
        colorBallBigRadius_ = colorBallBigRadius;
        colorBallAlpha_ = colorBallAlpha;
        colorBallBigAlpha_ = colorBallBigAlpha;
        fallbackColorBallRadius_ = fallbackColorBallRadius;
        fallbackColorBallAlpha_ = fallbackColorBallAlpha;
        renderFlags_ = renderFlags;
        sourceAspect_ = sourceAspect;
        clipTimeSeconds_ = clipTimeSeconds;
        life01_ = life01;
        starAngleSeed_ = starAngleSeed;
        glowFields_ = glowFields;
        sourceRectNormalized_ = sourceRectNormalized;
    }

    QSGTexture* texture() const { return texture_; }
    QSGTexture* brightBlockTexture() const { return brightBlockTexture_; }
    QSGTexture* bigBallTexture() const { return bigBallTexture_; }
    std::array<float, 4> starsUvTransform{};
    std::array<float, 2> patternOffset{};
    QSGTexture* starsTexture() const { return starsTexture_; }
    void setStarsTexture(QSGTexture* texture) { starsTexture_ = texture; }
    bool useTexture() const { return useTexture_; }
    float quadRadius() const { return quadRadius_; }
    float clipRadius() const { return clipRadius_; }
    float outerRadius() const { return outerRadius_; }
    float fireworkAlpha() const { return fireworkAlpha_; }
    float fireworkBrightness() const { return fireworkBrightness_; }
    float fireworkRotationDegrees() const { return fireworkRotationDegrees_; }
    float holeRadius() const { return holeRadius_; }
    float holeMaskRadius() const { return holeMaskRadius_; }
    float colorBallRadius() const { return colorBallRadius_; }
    float colorBallBigRadius() const { return colorBallBigRadius_; }
    float colorBallAlpha() const { return colorBallAlpha_; }
    float colorBallBigAlpha() const { return colorBallBigAlpha_; }
    float fallbackColorBallRadius() const { return fallbackColorBallRadius_; }
    float fallbackColorBallAlpha() const { return fallbackColorBallAlpha_; }
    float renderFlags() const { return renderFlags_; }
    float sourceAspect() const { return sourceAspect_; }
    float clipTimeSeconds() const { return clipTimeSeconds_; }
    float life01() const { return life01_; }
    float starAngleSeed() const { return starAngleSeed_; }
    const std::array<ModernFireworkGlowField, kModernFireworkGlowFieldCount>& glowFields() const { return glowFields_; }
    QRectF sourceRectNormalized() const { return sourceRectNormalized_; }

private:
    QSGTexture* starsTexture_ = nullptr;
    QSGTexture* texture_ = nullptr;
    QSGTexture* brightBlockTexture_ = nullptr;
    QSGTexture* bigBallTexture_ = nullptr;
    bool useTexture_ = false;
    float quadRadius_ = 0.0f;
    float clipRadius_ = 0.0f;
    float outerRadius_ = 0.0f;
    float fireworkAlpha_ = 0.0f;
    float fireworkBrightness_ = 1.0f;
    float fireworkRotationDegrees_ = 0.0f;
    float holeRadius_ = 0.0f;
    float holeMaskRadius_ = 0.0f;
    float colorBallRadius_ = 0.0f;
    float colorBallBigRadius_ = 0.0f;
    float colorBallAlpha_ = 0.0f;
    float colorBallBigAlpha_ = 0.0f;
    float fallbackColorBallRadius_ = 0.0f;
    float fallbackColorBallAlpha_ = 0.0f;
    float renderFlags_ = 0.0f;
    float sourceAspect_ = 1.0f;
    float clipTimeSeconds_ = 0.0f;
    float life01_ = 0.0f;
    float starAngleSeed_ = 0.0f;
    std::array<ModernFireworkGlowField, kModernFireworkGlowFieldCount> glowFields_{};
    QRectF sourceRectNormalized_;
};

bool ModernFireworkShader::updateUniformData(
    RenderState& state,
    QSGMaterial* newMaterial,
    QSGMaterial* oldMaterial
)
{
    Q_UNUSED(oldMaterial);

    QByteArray* uniformData = state.uniformData();
    if (uniformData == nullptr || uniformData->size() < kModernFireworkUniformBufferSize) {
        return false;
    }

    if (state.isMatrixDirty()) {
        const QMatrix4x4 matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), sizeof(float) * 16);
    }

    const auto* material = static_cast<const ModernFireworkMaterial*>(newMaterial);
    const float opacity = state.opacity();
    const float geometryScalars[3] = {
        material->quadRadius(),
        material->clipRadius(),
        material->outerRadius()
    };
    const float fireworkAndHole[4] = {
        material->fireworkAlpha(),
        material->fireworkRotationDegrees(),
        material->holeRadius(),
        material->holeMaskRadius()
    };
    const float colorBallSmall[4] = {
        material->colorBallRadius(),
        material->colorBallAlpha(),
        material->sourceAspect(),
        material->fallbackColorBallRadius()
    };
    const float colorBallBig[4] = {
        material->colorBallBigRadius(),
        material->colorBallBigAlpha(),
        material->fallbackColorBallAlpha(),
        material->renderFlags()
    };
    const float sourceRect[4] = {
        static_cast<float>(material->sourceRectNormalized().x()),
        static_cast<float>(material->sourceRectNormalized().y()),
        static_cast<float>(material->sourceRectNormalized().width()),
        static_cast<float>(material->sourceRectNormalized().height())
    };
    const float timing[4] = {
        material->clipTimeSeconds() / 1.067f,
        material->life01(),
        material->starAngleSeed(),
        material->clipTimeSeconds()
    };

    std::memcpy(uniformData->data() + 64, &opacity, sizeof(float));
    std::memcpy(uniformData->data() + 68, geometryScalars, sizeof(geometryScalars));
    std::memcpy(uniformData->data() + 80, fireworkAndHole, sizeof(fireworkAndHole));
    std::memcpy(uniformData->data() + 96, colorBallSmall, sizeof(colorBallSmall));
    std::memcpy(uniformData->data() + 112, colorBallBig, sizeof(colorBallBig));
    std::memcpy(uniformData->data() + 128, sourceRect, sizeof(sourceRect));
    std::memcpy(uniformData->data() + 144, timing, sizeof(timing));
    std::memcpy(
        uniformData->data() + 160,
        material->glowFields().data(),
        sizeof(ModernFireworkGlowField) * kModernFireworkGlowFieldCount);
    std::memcpy(uniformData->data() + 160 + sizeof(ModernFireworkGlowField) * kModernFireworkGlowFieldCount,
        material->starsUvTransform.data(), sizeof(float) * 4);
    const float brightnessAndPatternOffset[4] = {material->fireworkBrightness(),
        material->patternOffset[0], material->patternOffset[1], 0.0f};
    std::memcpy(uniformData->data() + 176 + sizeof(ModernFireworkGlowField) * kModernFireworkGlowFieldCount,
        brightnessAndPatternOffset, sizeof(brightnessAndPatternOffset));
    return true;
}

void ModernFireworkShader::updateSampledImage(
    RenderState& state,
    int binding,
    QSGTexture** texture,
    QSGMaterial* newMaterial,
    QSGMaterial* oldMaterial
)
{
    Q_UNUSED(oldMaterial);

    if ((binding != 1 && binding != 2 && binding != 3 && binding != 4) || texture == nullptr) {
        return;
    }

    const auto* material = static_cast<const ModernFireworkMaterial*>(newMaterial);
    *texture = binding == 1 ? material->texture()
        : binding == 2 ? material->brightBlockTexture()
        : binding == 3 ? material->bigBallTexture() : material->starsTexture();
    if (*texture != nullptr) {
        (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    }
}

bool ModernFireworkShader::updateGraphicsPipelineState(
    RenderState& state,
    GraphicsPipelineState* ps,
    QSGMaterial* newMaterial,
    QSGMaterial* oldMaterial
)
{
    Q_UNUSED(state);
    Q_UNUSED(newMaterial);
    Q_UNUSED(oldMaterial);

    if (ps == nullptr) {
        return false;
    }

    // Alpha controls coloured coverage; zero-coverage emission adds light to the stage.
    ps->blendEnable = true;
    ps->srcColor = GraphicsPipelineState::One;
    ps->dstColor = GraphicsPipelineState::OneMinusSrcAlpha;
    ps->separateBlendFactors = true;
    ps->srcAlpha = GraphicsPipelineState::One;
    ps->dstAlpha = GraphicsPipelineState::OneMinusSrcAlpha;
    ps->opColor = GraphicsPipelineState::BlendOp::Add;
    ps->opAlpha = GraphicsPipelineState::BlendOp::Add;
    return true;
}

class ModernFireworkNode final : public QSGNode
{
public:
    explicit ModernFireworkNode(QQuickWindow* window)
    {
        geometry_ = new QSGGeometry(modernFireworkVertexAttributes(), 4);
        geometry_->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        geometry_->setVertexDataPattern(QSGGeometry::DynamicPattern);
        postProcess_ = new PreviewModernFireworkPostProcess(window);
        appendChildNode(postProcess_);
        sourceNode_ = new QSGGeometryNode;
        postProcess_->sourceRoot()->appendChildNode(sourceNode_);
        sourceNode_->setGeometry(geometry_);
        sourceNode_->setFlag(QSGNode::OwnsGeometry, true);
        material_ = new ModernFireworkMaterial();
        sourceNode_->setMaterial(material_);
        sourceNode_->setFlag(QSGNode::OwnsMaterial, true);
        material_->setStarsTexture(postProcess_->starTexture());
    }

    void updateNode(
        const miacode::preview::scene::PreviewJudgeFireworkLayerState& layerState,
        QSGTexture* texture, QSGTexture* brightBlockTexture, QSGTexture* bigBallTexture,
        bool useTexture, const QRectF& normalizedSourceRect, float sourceAspect, float smoothBlendStart)
    {
        if (layerState.triggerSecond != previousTriggerSecond_) {
            starAngleSeed_ = modernFireworkStarSeed(layerState.triggerSecond);
            previousTriggerSecond_ = layerState.triggerSecond;
        }

        // The hole is a mask, not visible content; only drawn layers set the bounds.
        const qreal contentRadius = qMax(layerState.outerRadius,
            qMax(layerState.clipRadius * 1.26,
                qMax(layerState.colorBallRadius,
                    qMax(layerState.colorBallBigRadius, layerState.fallbackColorBallRadius))));
        const qreal resolvedQuadRadius = layerState.warmup ? kModernFireworkQuadRadiusMin
            : contentRadius * 1.045 + layerState.clipRadius * 0.012;
        int renderFlags = 0;
        if (layerState.drawFirework) {
            renderFlags |= kModernFireworkRenderFlagDrawStripe;
        }
        if (layerState.drawColorBallBig) {
            renderFlags |= kModernFireworkRenderFlagDrawBigBall;
        }
        if (layerState.drawColorBall) {
            renderFlags |= kModernFireworkRenderFlagDrawSmallBall;
        }
        if (useTexture) {
            renderFlags |= kModernFireworkRenderFlagUseTexture;
        }
        if (layerState.drawFallbackColorBall) {
            renderFlags |= kModernFireworkRenderFlagDrawFallbackBall;
        }
        if (layerState.warmup) {
            renderFlags |= kModernFireworkRenderFlagWarmup;
        }
        const float quadRadius = static_cast<float>(resolvedQuadRadius);
        const QRectF bounds(layerState.center.x() - quadRadius, layerState.center.y() - quadRadius,
                            quadRadius * 2.0, quadRadius * 2.0);
        if (bounds != previousBounds_) {
            auto* vertices = reinterpret_cast<ModernFireworkVertex*>(geometry_->vertexData());
            vertices[0] = {static_cast<float>(layerState.center.x() - quadRadius), static_cast<float>(layerState.center.y() - quadRadius), 0.0f, 0.0f};
            vertices[1] = {static_cast<float>(layerState.center.x() + quadRadius), static_cast<float>(layerState.center.y() - quadRadius), 1.0f, 0.0f};
            vertices[2] = {static_cast<float>(layerState.center.x() - quadRadius), static_cast<float>(layerState.center.y() + quadRadius), 0.0f, 1.0f};
            vertices[3] = {static_cast<float>(layerState.center.x() + quadRadius), static_cast<float>(layerState.center.y() + quadRadius), 1.0f, 1.0f};
            geometry_->markVertexDataDirty();
            previousBounds_ = bounds;
            sourceNode_->markDirty(QSGNode::DirtyGeometry);
        }
        const float animationTime = static_cast<float>(miacode::preview_gameplay::judgeEffectFireworkAnimationTime(layerState.clipTimeSeconds));
        const float exitScale = 1.0f + .045f * modernFireworkSmoothStep((animationTime + 1.0f / 30.0f - .90f) / .20f);
        const QRectF stageBounds(layerState.clipCenter.x() - layerState.clipRadius,
            layerState.clipCenter.y() - layerState.clipRadius,
            layerState.clipRadius * 2.0, layerState.clipRadius * 2.0);
        const QRectF captureBounds = bounds.intersected(stageBounds);
        material_->starsUvTransform = {
            float((bounds.x() - captureBounds.x()) / captureBounds.width()),
            float((bounds.y() - captureBounds.y()) / captureBounds.height()),
            float(bounds.width() / captureBounds.width()),
            float(bounds.height() / captureBounds.height())};
        postProcess_->update(bounds, stageBounds, layerState.clipRadius, exitScale, smoothBlendStart,
            modernFireworkStars(animationTime, layerState.clipRadius, starAngleSeed_));

        material_->patternOffset = {float(layerState.center.x() - layerState.clipCenter.x()),
            float(layerState.center.y() - layerState.clipCenter.y())};
        material_->setState(
            texture,
            brightBlockTexture,
            bigBallTexture,
            useTexture,
            quadRadius,
            static_cast<float>(layerState.clipRadius),
            static_cast<float>(layerState.outerRadius),
            static_cast<float>(layerState.fireworkAlpha),
            static_cast<float>(layerState.fireworkBrightness),
            static_cast<float>(layerState.fireworkRotationDegrees),
            static_cast<float>(layerState.holeRadius),
            static_cast<float>(layerState.holeMaskRadius),
            static_cast<float>(layerState.colorBallRadius),
            static_cast<float>(layerState.colorBallBigRadius),
            static_cast<float>(layerState.colorBallAlpha),
            static_cast<float>(layerState.colorBallBigAlpha),
            static_cast<float>(layerState.fallbackColorBallRadius),
            static_cast<float>(layerState.fallbackColorBallAlpha),
            static_cast<float>(renderFlags),
            sourceAspect,
            static_cast<float>(miacode::preview_gameplay::judgeEffectFireworkAnimationTime(
                layerState.clipTimeSeconds)),
            static_cast<float>(layerState.life01),
            starAngleSeed_,
            modernFireworkGlowFields(animationTime),
            normalizedSourceRect
        );
        sourceNode_->markDirty(QSGNode::DirtyMaterial);
    }

private:
    QSGGeometry* geometry_ = nullptr;
    ModernFireworkMaterial* material_ = nullptr;
    PreviewModernFireworkPostProcess* postProcess_ = nullptr;
    QSGGeometryNode* sourceNode_ = nullptr;
    QRectF previousBounds_;
    float starAngleSeed_ = 0.0f;
    qreal previousTriggerSecond_ = -1.0;
};

class ModernFireworkRootNode final : public QSGNode
{
public:
    explicit ModernFireworkRootNode(QQuickWindow* window) : window_(window)
    {
        contentSlot_ = new QSGNode();
        appendChildNode(contentSlot_);
    }

    ModernFireworkClipNode* ensureClipNode()
    {
        if (clipNode_ == nullptr) {
            clipNode_ = new ModernFireworkClipNode();
        }
        return clipNode_;
    }

    ModernFireworkNode* ensureFireworkNode()
    {
        if (fireworkNode_ == nullptr) {
            fireworkNode_ = new ModernFireworkNode(window_);
        }
        return fireworkNode_;
    }

    void useClip(bool enabled)
    {
        auto* fireworkNode = ensureFireworkNode();
        if (enabled) {
            auto* clipNode = ensureClipNode();
            if (clipNode->parent() != contentSlot_) {
                if (clipNode->parent() != nullptr) {
                    clipNode->parent()->removeChildNode(clipNode);
                }
                contentSlot_->appendChildNode(clipNode);
            }
            if (fireworkNode->parent() != clipNode) {
                if (fireworkNode->parent() != nullptr) {
                    fireworkNode->parent()->removeChildNode(fireworkNode);
                }
                clipNode->appendChildNode(fireworkNode);
            }
            pruneChildren(contentSlot_, clipNode);
            pruneChildren(clipNode, fireworkNode);
            return;
        }

        if (fireworkNode->parent() != contentSlot_) {
            if (fireworkNode->parent() != nullptr) {
                fireworkNode->parent()->removeChildNode(fireworkNode);
            }
            contentSlot_->appendChildNode(fireworkNode);
        }
        if (clipNode_ != nullptr) {
            if (clipNode_->parent() != nullptr) {
                clipNode_->parent()->removeChildNode(clipNode_);
            }
            delete clipNode_;
            clipNode_ = nullptr;
        }
        pruneChildren(contentSlot_, fireworkNode);
    }

    ModernFireworkClipNode* clipNode() const
    {
        return clipNode_;
    }

    ModernFireworkNode* fireworkNode() const
    {
        return fireworkNode_;
    }

private:
    static void pruneChildren(QSGNode* parent, QSGNode* keepChild)
    {
        if (parent == nullptr) {
            return;
        }

        QSGNode* child = parent->firstChild();
        while (child != nullptr) {
            QSGNode* next = child->nextSibling();
            if (child != keepChild) {
                parent->removeChildNode(child);
                delete child;
            }
            child = next;
        }
    }

    QQuickWindow* window_;
    QSGNode* contentSlot_ = nullptr;
    ModernFireworkClipNode* clipNode_ = nullptr;
    ModernFireworkNode* fireworkNode_ = nullptr;
};

ModernFireworkRootNode* ensureModernFireworkRoot(QSGNode* oldNode, QQuickWindow* window)
{
    if (auto* root = dynamic_cast<ModernFireworkRootNode*>(oldNode)) {
        return root;
    }
    return new ModernFireworkRootNode(window);
}

QRectF normalizedSourceRectFor(
    const QImage* sourceImage,
    const QRectF& sourceRect
)
{
    if (sourceImage == nullptr || sourceImage->isNull()) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }
    if (!sourceRect.isValid() || sourceRect.isEmpty()) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }

    const qreal width = qMax<qreal>(1.0, sourceImage->width());
    const qreal height = qMax<qreal>(1.0, sourceImage->height());
    return QRectF(
        sourceRect.x() / width,
        sourceRect.y() / height,
        sourceRect.width() / width,
        sourceRect.height() / height
    );
}

qreal sourceAspectFor(const QImage* sourceImage, const QRectF& sourceRect)
{
    if (sourceImage == nullptr || sourceImage->isNull()) {
        return 1.0;
    }
    const qreal sourceWidth = sourceRect.isValid() && !sourceRect.isEmpty()
        ? sourceRect.width()
        : static_cast<qreal>(sourceImage->width());
    const qreal sourceHeight = sourceRect.isValid() && !sourceRect.isEmpty()
        ? sourceRect.height()
        : static_cast<qreal>(sourceImage->height());
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0) {
        return 1.0;
    }
    return qMax<qreal>(0.01, sourceHeight / sourceWidth);
}

}  // namespace

QSGNode* PreviewQuickJudgeFireworkLayer::updateNode(
    QSGNode* oldNode,
    const miacode::preview::scene::PreviewFrameState& state,
    const miacode::preview::scene::PreviewPreparedSceneCache* preparedCache,
    const miacode::preview::scene::PreviewLayerWindowCursor* cursor,
    const QSize& renderSize,
    QQuickWindow* window,
    PreviewTextureRepository* textures
) const
{
    if (window == nullptr || textures == nullptr) {
        return nullptr;
    }

    const miacode::preview::scene::PreviewActiveMarkerView activeMarkers =
        preparedCache != nullptr && cursor != nullptr
        ? miacode::preview::scene::PreviewActiveMarkerView(state.noteMarkers, preparedCache->judgeFireworkLayer(), *cursor)
        : miacode::preview::scene::PreviewActiveMarkerView(state.noteMarkers);

    const miacode::preview::scene::PreviewJudgeFireworkLayerState layerState =
        miacode::preview::scene::buildPreviewJudgeFireworkLayerState(
            state,
            activeMarkers,
            miacode::preview::scene::playfieldRectForStage(
                miacode::preview::scene::stageRectForSize(renderSize),
                state.render.layoutSquareScale
            )
        );
    if (!layerState.active) {
        return nullptr;
    }
    if (state.render.fireworkStyle == PreviewFireworkStyle::Standard) {
        return updatePreviewStandardFireworkNode(oldNode, state, layerState,
            miacode::preview::scene::stageRectForSize(renderSize), window, textures);
    }

    const QImage* sourceImage = layerState.colorBallImage;
    const bool useTexture = sourceImage != nullptr && !sourceImage->isNull();
    QSGTexture* texture = textures->textureForImage(
        useTexture ? *sourceImage : fallbackModernFireworkTextureImage(),
        true
    );
    if (texture == nullptr) {
        return nullptr;
    }
    texture->setFiltering(QSGTexture::Linear);

    QSGTexture* brightBlockTexture = textures->textureForImage(
        modernFireworkBrightBlockImage(),
        true
    );
    if (brightBlockTexture == nullptr) {
        return nullptr;
    }
    brightBlockTexture->setFiltering(QSGTexture::Linear);

    const QImage& ballSource = useTexture ? *sourceImage : fallbackModernFireworkTextureImage();
    if (modernColorBallSourceKey_ != ballSource.cacheKey()) {
        modernColorBallRadianceImage_ = ballSource.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < modernColorBallRadianceImage_.height(); ++y) {
            auto* pixels = reinterpret_cast<QRgb*>(modernColorBallRadianceImage_.scanLine(y));
            for (int x = 0; x < modernColorBallRadianceImage_.width(); ++x) {
                pixels[x] = qRgb(qRed(pixels[x]), qGreen(pixels[x]), qBlue(pixels[x]));
            }
        }
        modernColorBallSourceKey_ = ballSource.cacheKey();
    }
    QSGTexture* bigBallTexture = textures->retainedTextureForImage(
        QStringLiteral("modern_firework_color_ball"), modernColorBallRadianceImage_, true);
    if (bigBallTexture == nullptr) return nullptr;
    bigBallTexture->setFiltering(QSGTexture::Linear);
    bigBallTexture->setMipmapFiltering(QSGTexture::Linear);
    bigBallTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    bigBallTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);

    auto* root = ensureModernFireworkRoot(oldNode, window);
    root->useClip(layerState.clipRadius > 0.0);
    if (root->clipNode() != nullptr) {
        root->clipNode()->updateClip(layerState.clipCenter, layerState.clipRadius);
    }

    root->ensureFireworkNode()->updateNode(
        layerState,
        texture,
        brightBlockTexture,
        bigBallTexture,
        useTexture,
        normalizedSourceRectFor(sourceImage, layerState.colorBallSourceRect),
        static_cast<float>(sourceAspectFor(sourceImage, layerState.colorBallSourceRect)),
        state.render.smoothBrightness
            ? float(miacode::preview_video::smoothBrightnessBlendStartRadius(
                layerState.clipRadius * 2.0, state.assets.layoutRingDiameterRatio)) : 0.0f
    );
    return root;
}
