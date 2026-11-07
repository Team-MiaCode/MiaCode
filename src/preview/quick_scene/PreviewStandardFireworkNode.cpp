#include "preview/quick_scene/PreviewStandardFireworkNode.h"

#include "core/scene/PreviewStandardFirework.h"
#include "core/video/PreviewVideoGeometryConfig.h"
#include "preview/quick_scene/PreviewTextureRepository.h"

#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <QtMath>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>

namespace {

using namespace miacode::preview::scene;

// Game captures show 64 horizontal cells and 128 vertical cells per playfield diameter.
constexpr double kFireworkPatternAspectRatio = 2.0;

struct StandardFireworkVertex {
    float x, y, u, v;
    float red, green, blue, alpha;
};

const QSGGeometry::AttributeSet& attributes()
{
    static const QSGGeometry::Attribute entries[] = {
        QSGGeometry::Attribute::create(0, 2, QSGGeometry::FloatType, true),
        QSGGeometry::Attribute::create(1, 2, QSGGeometry::FloatType),
        QSGGeometry::Attribute::create(2, 4, QSGGeometry::FloatType)
    };
    static const QSGGeometry::AttributeSet result = {3, sizeof(StandardFireworkVertex), entries};
    return result;
}

class StandardFireworkMaterial;

class StandardFireworkShader final : public QSGMaterialShader
{
public:
    explicit StandardFireworkShader(PreviewStandardFireworkMaterial kind)
    {
        setFlag(UpdatesGraphicsPipelineState);
        setShaderFileName(VertexStage, QStringLiteral(":/src/preview/quick_scene/shaders/PreviewStandardFirework.vert.qsb"));
        setShaderFileName(FragmentStage, kind == PreviewStandardFireworkMaterial::Composite
            ? QStringLiteral(":/src/preview/quick_scene/shaders/PreviewStandardFireworkOutput.frag.qsb")
            : kind == PreviewStandardFireworkMaterial::Rainbow
            ? QStringLiteral(":/src/preview/quick_scene/shaders/PreviewStandardFireworkRainbow.frag.qsb")
            : QStringLiteral(":/src/preview/quick_scene/shaders/PreviewStandardFireworkSprite.frag.qsb"));
    }
    bool updateUniformData(RenderState&, QSGMaterial*, QSGMaterial*) override;
    void updateSampledImage(RenderState&, int, QSGTexture**, QSGMaterial*, QSGMaterial*) override;
    bool updateGraphicsPipelineState(RenderState&, GraphicsPipelineState*, QSGMaterial*, QSGMaterial*) override;
};

class StandardFireworkMaterial final : public QSGMaterial
{
public:
    explicit StandardFireworkMaterial(PreviewStandardFireworkMaterial value) : kind(value)
    {
        setFlag(Blending);
        setFlag(NoBatching);
    }
    QSGMaterialType* type() const override
    {
        static QSGMaterialType types[5];
        return &types[static_cast<int>(kind)];
    }
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override
    {
        return new StandardFireworkShader(kind);
    }
    int compare(const QSGMaterial* other) const override
    {
        if (this == other) return 0;
        return std::less<const QSGMaterial*>{}(this, other) ? -1 : 1;
    }
    PreviewStandardFireworkMaterial kind;
    QSGTexture* sourceTexture = nullptr;
    QSGTexture* colorTexture = nullptr;
    float time = 0.0f;
    float exposure = 0.0f;
    float brightness = 1.0f;
    float opacity = 1.0f;
    QRectF patternRect;
    QPointF clipCenter;
    float clipRadius = 0.0f;
    float edgeFadeStart = 0.0f;
    float burstFadeStart = 0.0f;
    float burstFadeEnd = 0.0f;
};

bool StandardFireworkShader::updateUniformData(RenderState& state, QSGMaterial* value, QSGMaterial*)
{
    const auto* material = static_cast<StandardFireworkMaterial*>(value);
    auto* data = state.uniformData();
    if (state.isMatrixDirty()) {
        std::memcpy(data->data(), state.combinedMatrix().constData(), 64);
    }
    const float renderParameters[] = {state.opacity() * material->opacity,
        material->time, material->exposure, material->brightness};
    const float patternRect[] = {float(material->patternRect.x()), float(material->patternRect.y()),
        float(material->patternRect.width()), float(material->patternRect.height())};
    const float materialAndEdge[] = {float(material->kind), float(material->clipCenter.x()),
        float(material->clipCenter.y()), material->edgeFadeStart};
    std::memcpy(data->data() + 64, renderParameters, sizeof(renderParameters));
    std::memcpy(data->data() + 80, patternRect, sizeof(patternRect));
    std::memcpy(data->data() + 96, materialAndEdge, sizeof(materialAndEdge));
    const float clipBounds[] = {material->clipRadius, material->burstFadeStart, material->burstFadeEnd, 0};
    std::memcpy(data->data() + 112, clipBounds, sizeof(clipBounds));
    return true;
}

void StandardFireworkShader::updateSampledImage(RenderState& state, int binding, QSGTexture** texture,
    QSGMaterial* value, QSGMaterial*)
{
    const auto* material = static_cast<StandardFireworkMaterial*>(value);
    *texture = binding == 1 ? material->sourceTexture : material->colorTexture;
    if (*texture) (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
}

bool StandardFireworkShader::updateGraphicsPipelineState(RenderState&, GraphicsPipelineState* pipeline,
    QSGMaterial* value, QSGMaterial*)
{
    const auto kind = static_cast<StandardFireworkMaterial*>(value)->kind;
    pipeline->srcColor = GraphicsPipelineState::One;
    pipeline->dstColor = kind == PreviewStandardFireworkMaterial::Ring ? GraphicsPipelineState::One
        : kind == PreviewStandardFireworkMaterial::Star || kind == PreviewStandardFireworkMaterial::Rays ? GraphicsPipelineState::OneMinusSrcColor
        : GraphicsPipelineState::OneMinusSrcAlpha;
    pipeline->separateBlendFactors = true;
    pipeline->srcAlpha = GraphicsPipelineState::One;
    pipeline->dstAlpha = GraphicsPipelineState::OneMinusSrcAlpha;
    return true;
}

class StandardFireworkSpriteBatch final : public QSGGeometryNode
{
public:
    explicit StandardFireworkSpriteBatch(PreviewStandardFireworkMaterial type) : geometry(attributes(), 0), material(type)
    {
        geometry.setDrawingMode(QSGGeometry::DrawTriangles);
        geometry.setVertexDataPattern(QSGGeometry::DynamicPattern);
        setGeometry(&geometry);
        setMaterial(&material);
    }

    static void writeQuad(StandardFireworkVertex* vertices, const QPointF& center, double diameter,
        double rotation, const std::array<float, 4>& color)
    {
        constexpr float coordinates[6][2] = {{0, 0}, {1, 0}, {0, 1}, {0, 1}, {1, 0}, {1, 1}};
        const double cosine = std::cos(rotation);
        const double sine = std::sin(rotation);
        for (int index = 0; index < 6; ++index) {
            const float u = coordinates[index][0], v = coordinates[index][1];
            const double x = (u - 0.5) * diameter, y = (v - 0.5) * diameter;
            vertices[index] = {float(center.x() + cosine * x - sine * y),
                float(center.y() + sine * x + cosine * y), u, v,
                color[0], color[1], color[2], color[3]};
        }
    }

    void updateSprites(const QVector<PreviewStandardFireworkSprite>& sprites,
        const QPointF& center, double scale)
    {
        int count = 0;
        for (const auto& sprite : sprites) if (sprite.material == material.kind) ++count;
        if (geometry.vertexCount() != count * 6) geometry.allocate(count * 6);
        auto* vertices = static_cast<StandardFireworkVertex*>(geometry.vertexData());
        for (const auto& sprite : sprites) {
            if (sprite.material != material.kind) continue;
            writeQuad(vertices, center + sprite.offset * scale, sprite.diameter * scale,
                sprite.rotation, sprite.color);
            material.exposure = sprite.exposure;
            vertices += 6;
        }
        geometry.markVertexDataDirty();
        markDirty(DirtyGeometry | DirtyMaterial);
    }

    void setOutputRect(const QRectF& rect)
    {
        if (geometry.vertexCount() != 6) geometry.allocate(6);
        auto* vertices = static_cast<StandardFireworkVertex*>(geometry.vertexData());
        writeQuad(vertices, rect.center(), 1.0, 0.0, {1, 1, 1, 1});
        for (int index = 0; index < 6; ++index) {
            vertices[index].x = rect.x() + vertices[index].u * rect.width();
            vertices[index].y = rect.y() + vertices[index].v * rect.height();
        }
        geometry.markVertexDataDirty();
        markDirty(DirtyGeometry | DirtyMaterial);
    }

    QSGGeometry geometry;
    StandardFireworkMaterial material;
};

class StandardFireworkNode final : public QSGNode
{
public:
    explicit StandardFireworkNode(QQuickWindow* window) : window_(window)
    {
        auto* context = QQuickWindowPrivate::get(window)->context;
        layer_ = context->sceneGraphContext()->createLayer(context);
        layer_->setItem(&sourceRoot_);
        layer_->setLive(false);
        layer_->setRecursive(false);
        layer_->setFormat(QSGLayer::RGBA8);
        layer_->setHasMipmaps(false);
        layer_->setMirrorVertical(true);
        layer_->setFiltering(QSGTexture::Linear);
        output_ = new StandardFireworkSpriteBatch(PreviewStandardFireworkMaterial::Composite);
        output_->material.sourceTexture = layer_;
        appendChildNode(output_);
        for (int index = 0; index < 4; ++index) {
            constexpr PreviewStandardFireworkMaterial materials[] = {
                PreviewStandardFireworkMaterial::Rainbow, PreviewStandardFireworkMaterial::Ring,
                PreviewStandardFireworkMaterial::Star, PreviewStandardFireworkMaterial::Rays
            };
            batches_[index] = new StandardFireworkSpriteBatch(materials[index]);
            sourceRoot_.appendChildNode(batches_[index]);
        }
        setFlag(UsePreprocess);
    }

    ~StandardFireworkNode() override { delete layer_; }

    void update(const PreviewFrameState& state, const PreviewJudgeFireworkLayerState& layerState,
        const QRectF& stageRect, PreviewTextureRepository* textures)
    {
        const auto effect = state.judgeEffect.standardFirework.effect;
        if (effect_ != effect || triggerSecond_ != layerState.triggerSecond || randomSeed_ != layerState.randomSeed) {
            effect_ = effect;
            triggerSecond_ = layerState.triggerSecond;
            randomSeed_ = layerState.randomSeed;
            burst_ = effect->createBurst(randomSeed_);
        }
        const auto sprites = burst_.sample(layerState.clipTimeSeconds);
        const auto textureForImage = [textures](const QImage& image) {
            QSGTexture* texture = textures->textureForImage(image, true);
            if (texture) {
                texture->setFiltering(QSGTexture::Linear);
                texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
                texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
            }
            return texture;
        };
        const auto& assets = state.judgeEffect.standardFirework;
        QSGTexture* starTexture = textureForImage(assets.starImage);
        QSGTexture* raysTexture = textureForImage(assets.raysImage);
        QSGTexture* ringMaskTexture = textureForImage(assets.ringMaskImage);
        QSGTexture* ringColorTexture = textureForImage(assets.ringColorImage);
        // Keep the screen-aligned grid fixed when the window or burst origin moves.
        const double diameter = layerState.clipRadius * 2.0;
        double burstRadius = 0.0;
        for (const auto& sprite : sprites) {
            if (sprite.material == PreviewStandardFireworkMaterial::Rainbow) {
                burstRadius = qMin(layerState.clipRadius, sprite.diameter * diameter / 2000.0);
                break;
            }
        }
        const double patternWidth = diameter * kFireworkPatternAspectRatio;
        const QRectF patternRect(layerState.clipCenter.x() - patternWidth * 0.5,
            layerState.clipCenter.y() - layerState.clipRadius, patternWidth, diameter);
        for (auto* batch : batches_) {
            auto& material = batch->material;
            material.patternRect = patternRect;
            material.clipCenter = layerState.center;
            material.burstFadeStart = float(burstRadius * 0.80);
            material.burstFadeEnd = float(burstRadius);
            material.time = float(std::fmod(state.playheadSeconds, 1000.0));
            material.sourceTexture = material.kind == PreviewStandardFireworkMaterial::Ring ? ringMaskTexture
                : material.kind == PreviewStandardFireworkMaterial::Rays ? raysTexture : starTexture;
            material.colorTexture = ringColorTexture;
            batch->updateSprites(sprites, layerState.center, layerState.clipRadius * 2.0 / 1000.0);
        }
        // Composite particles before applying stage coverage.
        const QRectF bounds = stageRect;
        layer_->setRect(bounds);
        const double dpr = window_->effectiveDevicePixelRatio();
        layer_->setSize(QSize(qCeil(bounds.width() * dpr), qCeil(bounds.height() * dpr)));
        layer_->markDirtyTexture();
        layer_->scheduleUpdate();
        output_->material.opacity = layerState.warmup ? 0.0f : 1.0f;
        output_->material.brightness = layerState.fireworkBrightness;
        output_->material.clipCenter = layerState.clipCenter;
        output_->material.clipRadius = layerState.clipRadius;
        output_->material.edgeFadeStart = state.render.smoothBrightness
            ? float(miacode::preview_video::smoothBrightnessBlendStartRadius(
                layerState.clipRadius * 2.0, state.assets.layoutRingDiameterRatio)) : 0.0f;
        output_->setOutputRect(bounds);
    }

    void preprocess() override { layer_->updateTexture(); }

private:
    QQuickWindow* window_;
    QSGRootNode sourceRoot_;
    QSGLayer* layer_;
    StandardFireworkSpriteBatch* output_;
    std::array<StandardFireworkSpriteBatch*, 4> batches_{};
    std::shared_ptr<const PreviewStandardFireworkEffect> effect_;
    PreviewStandardFireworkBurst burst_;
    double triggerSecond_ = std::numeric_limits<double>::quiet_NaN();
    quint32 randomSeed_ = 0;
};

}  // namespace

QSGNode* updatePreviewStandardFireworkNode(QSGNode* oldNode,
    const PreviewFrameState& state, const PreviewJudgeFireworkLayerState& layerState,
    const QRectF& stageRect, QQuickWindow* window, PreviewTextureRepository* textures)
{
    if (!state.judgeEffect.standardFirework.effect) return nullptr;
    auto* node = dynamic_cast<StandardFireworkNode*>(oldNode);
    if (!node) node = new StandardFireworkNode(window);
    node->update(state, layerState, stageRect, textures);
    return node;
}
