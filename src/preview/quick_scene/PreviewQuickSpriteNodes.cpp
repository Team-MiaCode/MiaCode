#include "preview/quick_scene/PreviewQuickSpriteNodes.h"

#include "preview/quick_scene/PreviewQuickSpriteBatchPolicy.h"
#include "preview/quick_scene/PreviewQuickSpriteSequence.h"
#include "preview/quick_scene/PreviewTextureRepository.h"
#include "core/scene/PreviewAnimatedSpriteHelpers.h"

#include <QHash>
#include <QPainter>
#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGNode>
#include <QSGTexture>
#include <QVector>
#include <QtMath>

#include <cstring>
#include <algorithm>
#include <array>
#include <memory>

namespace {

using miacode::preview::scene::PreviewAnimatedSpriteEffect;
using miacode::preview::scene::PreviewSpriteDescriptor;
using miacode::preview::quick_scene::SpriteFamilyKey;

struct PreviewQuickSpriteVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float opacity = 1.0f;
    float effect = 0.0f;
};

struct OrderedRunScratch {
    SpriteFamilyKey familyKey = 0;
    QSGTexture* texture = nullptr;
    QVector<PreviewQuickSpriteVertex> vertices;
    int spriteCount = 0;
    struct RepeatedLayer {
        std::array<float, 4> rect;
        std::array<float, 4> uv;
        std::array<float, 4> transform;
        std::array<float, 4> clamp;
    };
    struct Repetition {
        std::array<float, 4> counts{};
        std::array<RepeatedLayer, miacode::preview::quick_scene::kPreviewRepeatedSpriteLayerLimit> layers{};
    };
    std::shared_ptr<Repetition> repetition;
    bool repetitionUpper = false;
};

constexpr int kPreviewQuickSpriteUniformBufferSize = 80;

const QSGGeometry::AttributeSet& previewQuickSpriteVertexAttributes(bool repeated = false)
{
    static QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::create(2, 1, QSGGeometry::FloatType),
        QSGGeometry::Attribute::create(3, 1, QSGGeometry::FloatType),
    };
    static QSGGeometry::AttributeSet attributeSet = {
        4,
        static_cast<int>(sizeof(PreviewQuickSpriteVertex)),
        attributes
    };
    static QSGGeometry::AttributeSet repeatedAttributeSet = {
        1, static_cast<int>(sizeof(PreviewQuickSpriteVertex)), attributes
    };
    return repeated ? repeatedAttributeSet : attributeSet;
}

QRectF normalizedSourceRect(
    QSGTexture* texture,
    const PreviewSpriteDescriptor& sprite
)
{
    if (texture == nullptr) {
        return QRectF();
    }
    if (sprite.sourceRect.isValid() && !sprite.sourceRect.isEmpty()) {
        return texture->convertToNormalizedSourceRect(sprite.sourceRect);
    }
    return texture->normalizedTextureSubRect();
}

QPointF rotateLocalPoint(const QPointF& point, qreal degrees)
{
    if (qFuzzyIsNull(degrees)) {
        return point;
    }

    const qreal radians = qDegreesToRadians(degrees);
    const qreal sinValue = qSin(radians);
    const qreal cosValue = qCos(radians);
    return QPointF(
        point.x() * cosValue - point.y() * sinValue,
        point.x() * sinValue + point.y() * cosValue
    );
}

void writeSpriteVertices(
    PreviewQuickSpriteVertex* vertices,
    int baseIndex,
    const PreviewSpriteDescriptor& sprite,
    const QRectF& uvRect
)
{
    if (vertices == nullptr || baseIndex < 0) {
        return;
    }

    const qreal halfWidth = sprite.width / 2.0;
    const qreal halfHeight = sprite.height / 2.0;
    const QPointF localTopLeft(-halfWidth, -halfHeight);
    const QPointF localTopRight(halfWidth, -halfHeight);
    const QPointF localBottomLeft(-halfWidth, halfHeight);
    const QPointF localBottomRight(halfWidth, halfHeight);

    const QPointF topLeft = sprite.center + rotateLocalPoint(localTopLeft, sprite.rotationDegrees);
    const QPointF topRight = sprite.center + rotateLocalPoint(localTopRight, sprite.rotationDegrees);
    const QPointF bottomLeft = sprite.center + rotateLocalPoint(localBottomLeft, sprite.rotationDegrees);
    const QPointF bottomRight = sprite.center + rotateLocalPoint(localBottomRight, sprite.rotationDegrees);

    const float leftU = static_cast<float>(uvRect.left());
    const float rightU = static_cast<float>(uvRect.left() + uvRect.width());
    const float topV = static_cast<float>(uvRect.top());
    const float bottomV = static_cast<float>(uvRect.top() + uvRect.height());
    const float opacity = static_cast<float>(sprite.opacity);
    const float effect = static_cast<float>(sprite.effect);

    vertices[baseIndex + 0] = {static_cast<float>(topLeft.x()), static_cast<float>(topLeft.y()), leftU, topV, opacity, effect};
    vertices[baseIndex + 1] = {static_cast<float>(topRight.x()), static_cast<float>(topRight.y()), rightU, topV, opacity, effect};
    vertices[baseIndex + 2] = {static_cast<float>(bottomLeft.x()), static_cast<float>(bottomLeft.y()), leftU, bottomV, opacity, effect};
    vertices[baseIndex + 3] = {static_cast<float>(topRight.x()), static_cast<float>(topRight.y()), rightU, topV, opacity, effect};
    vertices[baseIndex + 4] = {static_cast<float>(bottomRight.x()), static_cast<float>(bottomRight.y()), rightU, bottomV, opacity, effect};
    vertices[baseIndex + 5] = {static_cast<float>(bottomLeft.x()), static_cast<float>(bottomLeft.y()), leftU, bottomV, opacity, effect};
}

class PreviewQuickSpriteBatchRootNode final : public QSGNode
{
public:
    QVector<OrderedRunScratch> runs;
    QHash<SpriteFamilyKey, int> familyVertexReserveHint;
    QVector<QSGNode*> scratchChildren;
    QImage atlasImage;
    QHash<qint64, QRect> atlasRects;

    void prepareAtlas(const miacode::preview::scene::PreviewSpriteDescriptors& sprites)
    {
        // Keep painter order while sharing one texture across alternating WiFi rows.
        // Thousands of separate overlapping QSG nodes otherwise cause quadratic
        // batch invalidation while the GUI thread waits for scene synchronization.
        if (sprites.size() < 512) return;
        QHash<qint64, const QImage*> images;
        bool covered = !atlasImage.isNull();
        int transitions = 0;
        qint64 previousKey = 0;
        for (const auto& sprite : sprites) {
            if (!sprite.cacheable || sprite.image == nullptr || sprite.image->isNull()) continue;
            const qint64 key = sprite.image->cacheKey();
            if (key != previousKey) ++transitions;
            previousKey = key;
            if (!atlasRects.contains(key)) covered = false;
            images.insert(key, sprite.image);
            if (images.size() > 64) return;
        }
        if (covered || transitions < 256 || images.size() < 2) return;

        constexpr int side = 2048;
        constexpr int padding = 8;
        auto orderedImages = images.values();
        std::sort(orderedImages.begin(), orderedImages.end(), [](const QImage* a, const QImage* b) {
            return a->height() > b->height();
        });
        QHash<qint64, QRect> rects;
        int x = 0;
        int y = 0;
        int rowHeight = 0;
        for (const QImage* image : orderedImages) {
            const int width = image->width() + padding * 2;
            const int height = image->height() + padding * 2;
            if (width > side || height > side) return;
            if (x + width > side) {
                x = 0;
                y += rowHeight;
                rowHeight = 0;
            }
            if (y + height > side) return;
            rects.insert(image->cacheKey(), QRect(x + padding, y + padding, image->width(), image->height()));
            x += width;
            rowHeight = qMax(rowHeight, height);
        }
        QImage atlas(side, y + rowHeight, QImage::Format_ARGB32_Premultiplied);
        atlas.fill(Qt::transparent);
        QPainter painter(&atlas);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        for (const QImage* image : orderedImages) {
            const QRect rect = rects.value(image->cacheKey());
            painter.drawImage(rect.topLeft(), *image);
            // Replicate the image boundary for bilinear and footprint sampling.
            for (int border = 1; border <= padding; ++border) {
                painter.drawImage(QRect(rect.left() - border, rect.top(), 1, rect.height()), *image,
                                  QRect(0, 0, 1, image->height()));
                painter.drawImage(QRect(rect.right() + border, rect.top(), 1, rect.height()), *image,
                                  QRect(image->width() - 1, 0, 1, image->height()));
            }
            painter.drawImage(QRect(rect.left() - padding, rect.top() - padding, rect.width() + padding * 2, padding),
                              atlas, QRect(rect.left() - padding, rect.top(), rect.width() + padding * 2, 1));
            painter.drawImage(QRect(rect.left() - padding, rect.bottom() + 1, rect.width() + padding * 2, padding),
                              atlas, QRect(rect.left() - padding, rect.bottom(), rect.width() + padding * 2, 1));
        }
        painter.end();
        atlasImage = std::move(atlas);
        atlasRects = std::move(rects);
    }
};

PreviewQuickSpriteBatchRootNode* ensurePreviewQuickSpriteRoot(QSGNode* oldNode)
{
    if (auto* root = dynamic_cast<PreviewQuickSpriteBatchRootNode*>(oldNode)) {
        return root;
    }
    delete oldNode;
    return new PreviewQuickSpriteBatchRootNode();
}

bool buildRepeatedRun(
    const miacode::preview::scene::PreviewSpriteDescriptors& sprites,
    int first,
    const miacode::preview::quick_scene::PreviewSpriteRepetition& repetition,
    const PreviewQuickSpriteBatchRootNode& root,
    QSGTexture* atlasTexture,
    PreviewTextureRepository* textures,
    OrderedRunScratch* run)
{
    auto data = std::make_shared<OrderedRunScratch::Repetition>();
    data->counts = {static_cast<float>(repetition.period), static_cast<float>(repetition.count), 0, 0};
    QSGTexture* sharedTexture = nullptr;
    QRectF bounds;
    for (int index = 0; index < repetition.period; ++index) {
        const auto& sprite = sprites[first + index];
        if (sprite.image == nullptr || sprite.image->isNull() || sprite.width <= 0 || sprite.height <= 0
            || sprite.opacity <= 0 || !sprite.cacheable) return false;
        const auto atlasIt = root.atlasRects.constFind(sprite.image->cacheKey());
        const bool usesAtlas = atlasTexture != nullptr && atlasIt != root.atlasRects.constEnd();
        QSGTexture* texture = usesAtlas ? atlasTexture : textures->textureForImage(*sprite.image);
        if (texture == nullptr || (sharedTexture != nullptr && sharedTexture != texture)) return false;
        sharedTexture = texture;
        const QRectF source = sprite.sourceRect.isValid() && !sprite.sourceRect.isEmpty()
            ? sprite.sourceRect : QRectF(QPointF(), sprite.image->size());
        const QPointF offset = usesAtlas ? atlasIt->topLeft() : QPointF();
        const QRectF uv = texture->convertToNormalizedSourceRect(source.translated(offset));
        const QRectF clamp = texture->convertToNormalizedSourceRect(
            QRectF(offset + QPointF(0.5, 0.5), QSizeF(sprite.image->width() - 1, sprite.image->height() - 1)));
        auto& layer = data->layers[index];
        layer.rect = {static_cast<float>(sprite.center.x()), static_cast<float>(sprite.center.y()),
                      static_cast<float>(1.0 / sprite.width), static_cast<float>(1.0 / sprite.height)};
        layer.uv = {static_cast<float>(uv.x()), static_cast<float>(uv.y()),
                    static_cast<float>(uv.width()), static_cast<float>(uv.height())};
        const qreal radians = qDegreesToRadians(sprite.rotationDegrees);
        layer.transform = {static_cast<float>(qCos(radians)), static_cast<float>(qSin(radians)),
                           static_cast<float>(sprite.opacity), static_cast<float>(sprite.effect)};
        layer.clamp = {static_cast<float>(clamp.left()), static_cast<float>(clamp.top()),
                       static_cast<float>(clamp.right()), static_cast<float>(clamp.bottom())};
        const qreal halfWidth = (qAbs(qCos(radians)) * sprite.width + qAbs(qSin(radians)) * sprite.height) / 2;
        const qreal halfHeight = (qAbs(qSin(radians)) * sprite.width + qAbs(qCos(radians)) * sprite.height) / 2;
        bounds = bounds.united(QRectF(sprite.center - QPointF(halfWidth, halfHeight), QSizeF(halfWidth * 2, halfHeight * 2)));
    }
    run->familyKey = reinterpret_cast<quintptr>(sharedTexture);
    run->texture = sharedTexture;
    sharedTexture->setFiltering(QSGTexture::Linear);
    run->repetition = std::move(data);
    run->spriteCount = repetition.period * repetition.count;
    run->vertices.resize(6);
    PreviewSpriteDescriptor quad;
    quad.center = bounds.center();
    quad.width = bounds.width();
    quad.height = bounds.height();
    writeSpriteVertices(run->vertices.data(), 0, quad, QRectF(0, 0, 1, 1));
    return true;
}

struct PreviewQuickSpriteBatchNodeUpdateProfile {
    int requiredVertexCount = 0;
    int allocatedVertexCapacity = 0;
    bool geometryReallocated = false;
};

class PreviewQuickSpriteMaterialShader final : public QSGMaterialShader
{
public:
    explicit PreviewQuickSpriteMaterialShader(bool repeated, bool upper) : repeated_(repeated), upper_(upper)
    {
        const QString name = repeated ? QStringLiteral("PreviewSpriteRepeatMaterial") : QStringLiteral("PreviewSpriteMaterial");
        setShaderFileName(VertexStage, QStringLiteral(":/src/preview/quick_scene/shaders/%1.vert.qsb").arg(name));
        setShaderFileName(FragmentStage, QStringLiteral(":/src/preview/quick_scene/shaders/%1.frag.qsb").arg(name));
        if (repeated) setFlag(UpdatesGraphicsPipelineState);
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override;
    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override;
    bool updateGraphicsPipelineState(RenderState&, GraphicsPipelineState* state, QSGMaterial*, QSGMaterial*) override
    {
        if (!repeated_) return false;
        state->blendEnable = true;
        state->separateBlendFactors = true;
        state->srcColor = state->dstColor = state->srcAlpha = state->dstAlpha = GraphicsPipelineState::One;
        state->opColor = state->opAlpha = upper_ ? GraphicsPipelineState::BlendOp::Min : GraphicsPipelineState::BlendOp::Max;
        return true;
    }

private:
    bool repeated_ = false;
    bool upper_ = false;
};

class PreviewQuickSpriteMaterial final : public QSGMaterial
{
public:
    explicit PreviewQuickSpriteMaterial(bool repeated, bool upper) : repeated_(repeated), upper_(upper)
    {
        setFlag(Blending, true);
        setFlag(RequiresFullMatrix, true);
        setFlag(NoBatching, true);
    }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType materialType;
        static QSGMaterialType repeatedMaterialType;
        static QSGMaterialType upperMaterialType;
        return repeated_ ? (upper_ ? &upperMaterialType : &repeatedMaterialType) : &materialType;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override
    {
        return new PreviewQuickSpriteMaterialShader(repeated_, upper_);
    }

    int compare(const QSGMaterial* other) const override
    {
        const auto* otherMaterial = static_cast<const PreviewQuickSpriteMaterial*>(other);
        if (texture_ == otherMaterial->texture_) {
            return 0;
        }
        return texture_ < otherMaterial->texture_ ? -1 : 1;
    }

    void setBatchState(QSGTexture* texture, qreal wave, qreal absWave)
    {
        texture_ = texture;
        wave_ = wave;
        absWave_ = absWave;
    }

    QSGTexture* texture() const
    {
        return texture_;
    }

    qreal wave() const
    {
        return wave_;
    }

    qreal absWave() const
    {
        return absWave_;
    }

    std::shared_ptr<OrderedRunScratch::Repetition> repetition;
    bool repetitionUpper() const { return upper_; }

private:
    QSGTexture* texture_ = nullptr;
    qreal wave_ = 0.0;
    qreal absWave_ = 0.0;
    bool repeated_ = false;
    bool upper_ = false;
};

bool PreviewQuickSpriteMaterialShader::updateUniformData(
    RenderState& state,
    QSGMaterial* newMaterial,
    QSGMaterial* oldMaterial
)
{
    Q_UNUSED(oldMaterial);

    QByteArray* uniformData = state.uniformData();
    if (uniformData == nullptr || uniformData->size() < kPreviewQuickSpriteUniformBufferSize) {
        return false;
    }

    if (state.isMatrixDirty()) {
        const QMatrix4x4 matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), sizeof(float) * 16);
    }

    const auto* material = static_cast<const PreviewQuickSpriteMaterial*>(newMaterial);

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, sizeof(float));
    }

    const float wave = static_cast<float>(material->wave());
    const float absWave = static_cast<float>(material->absWave());
    const float effect = 0.0f;
    std::memcpy(uniformData->data() + 68, &wave, sizeof(float));
    std::memcpy(uniformData->data() + 72, &absWave, sizeof(float));
    std::memcpy(uniformData->data() + 76, &effect, sizeof(float));
    if (material->repetition != nullptr) {
        static_assert(sizeof(OrderedRunScratch::Repetition) == 16 + 64 * 32);
        if (uniformData->size() < kPreviewQuickSpriteUniformBufferSize + sizeof(OrderedRunScratch::Repetition)) return false;
        std::memcpy(uniformData->data() + kPreviewQuickSpriteUniformBufferSize,
                    material->repetition.get(), sizeof(OrderedRunScratch::Repetition));
        const float upper = material->repetitionUpper() ? 1.0f : 0.0f;
        std::memcpy(uniformData->data() + 88, &upper, sizeof(float));
    }
    return true;
}

void PreviewQuickSpriteMaterialShader::updateSampledImage(
    RenderState& state,
    int binding,
    QSGTexture** texture,
    QSGMaterial* newMaterial,
    QSGMaterial* oldMaterial
)
{
    Q_UNUSED(state);
    Q_UNUSED(oldMaterial);

    if (binding != 1 || texture == nullptr) {
        return;
    }

    const auto* material = static_cast<const PreviewQuickSpriteMaterial*>(newMaterial);
    *texture = material->texture();
    if (*texture != nullptr) {
        (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    }
}

class PreviewQuickSpriteBatchNode final : public QSGGeometryNode
{
public:
    explicit PreviewQuickSpriteBatchNode(bool repeated, bool upper) : repeated_(repeated), upper_(upper)
    {
        geometry_ = new QSGGeometry(previewQuickSpriteVertexAttributes(repeated), 0);
        geometry_->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry_->setVertexDataPattern(QSGGeometry::DynamicPattern);
        setGeometry(geometry_);
        setFlag(QSGNode::OwnsGeometry, true);

        material_ = new PreviewQuickSpriteMaterial(repeated, upper);
        setMaterial(material_);
        setFlag(QSGNode::OwnsMaterial, true);
    }

    bool isRepeated() const { return repeated_; }
    bool isUpper() const { return upper_; }

    PreviewQuickSpriteBatchNodeUpdateProfile updateBatch(const OrderedRunScratch& run, qreal wave, qreal absWave)
    {
        PreviewQuickSpriteBatchNodeUpdateProfile profile;
        profile.requiredVertexCount = run.vertices.size();

        if (profile.requiredVertexCount > allocatedVertexCapacity_) {
            allocatedVertexCapacity_ =
                miacode::preview::quick_scene::previewQuickSpriteAlignedVertexCapacity(profile.requiredVertexCount);
            geometry_->allocate(allocatedVertexCapacity_);
            underuseFrameCount_ = 0;
            profile.geometryReallocated = true;
        } else {
            underuseFrameCount_ = miacode::preview::quick_scene::previewQuickSpriteNextUnderuseFrameCount(
                profile.requiredVertexCount,
                allocatedVertexCapacity_,
                underuseFrameCount_);
            if (miacode::preview::quick_scene::previewQuickSpriteShouldShrink(
                    profile.requiredVertexCount,
                    allocatedVertexCapacity_,
                    underuseFrameCount_)) {
                allocatedVertexCapacity_ =
                    miacode::preview::quick_scene::previewQuickSpriteAlignedVertexCapacity(profile.requiredVertexCount);
                geometry_->allocate(allocatedVertexCapacity_);
                underuseFrameCount_ = 0;
                profile.geometryReallocated = true;
            }
        }

        auto* geometryVertices = reinterpret_cast<PreviewQuickSpriteVertex*>(geometry_->vertexData());
        if (profile.requiredVertexCount > 0) {
            std::memcpy(
                geometryVertices,
                run.vertices.constData(),
                static_cast<size_t>(profile.requiredVertexCount) * sizeof(PreviewQuickSpriteVertex)
            );

            if (profile.requiredVertexCount < allocatedVertexCapacity_) {
                PreviewQuickSpriteVertex filler = run.vertices.constLast();
                filler.opacity = 0.0f;
                filler.effect = 0.0f;
                miacode::preview::quick_scene::fillPreviewQuickDegenerateVertexTail(
                    geometryVertices,
                    profile.requiredVertexCount,
                    allocatedVertexCapacity_,
                    filler
                );
            }
        }
        geometry_->markVertexDataDirty();

        const bool textureChanged = lastTexture_ != run.texture;
        material_->setBatchState(run.texture, wave, absWave);
        material_->repetition = run.repetition;
        // Only flag DirtyMaterial when the texture binding actually changed. Per-frame
        // wave/absWave uniform updates are picked up by updateUniformData() regardless of
        // this flag, and adjacent batches always have distinct textures so Qt's batcher
        // would never merge them either way. Setting DirtyMaterial unconditionally forces
        // Qt to re-evaluate compare() and refresh the RHI resource binding every frame —
        // wasted work in steady state.
        const QSGNode::DirtyState dirty = textureChanged
            ? (QSGNode::DirtyGeometry | QSGNode::DirtyMaterial)
            : QSGNode::DirtyGeometry;
        markDirty(dirty);
        lastTexture_ = run.texture;
        profile.allocatedVertexCapacity = allocatedVertexCapacity_;
        return profile;
    }

private:
    QSGGeometry* geometry_ = nullptr;
    PreviewQuickSpriteMaterial* material_ = nullptr;
    int allocatedVertexCapacity_ = 0;
    int underuseFrameCount_ = 0;
    QSGTexture* lastTexture_ = nullptr;
    bool repeated_ = false;
    bool upper_ = false;
};

}  // namespace

QSGNode* buildPreviewSpriteNodeTree(
    QSGNode* oldNode,
    const miacode::preview::scene::PreviewSpriteDescriptors& sprites,
    QQuickWindow* window,
    PreviewTextureRepository* textures,
    double playheadSeconds,
    const char* profileLayerName
)
{
    if (window == nullptr || textures == nullptr) {
        delete oldNode;
        return nullptr;
    }

    // Empty-layer fast path: skip root allocation + batch-run scan for layers that have no
    // sprites this frame. 9 of the 15 layers are typically empty for non-touch charts, so
    // this saves ~9 new/delete churns per frame plus the (zero-iteration) scan loop.
    if (sprites.isEmpty()) {
        if (textures != nullptr) {
            PreviewSpriteBatchFrameProfile emptyProfile;
            textures->noteSpriteBatchStats(profileLayerName, emptyProfile);
        }
        delete oldNode;
        return nullptr;
    }

    auto* root = ensurePreviewQuickSpriteRoot(oldNode);
    root->prepareAtlas(sprites);
    QVector<OrderedRunScratch>& runs = root->runs;
    QSGTexture* atlasTexture = root->atlasImage.isNull()
        ? nullptr : textures->textureForImage(root->atlasImage);

    int visibleSpriteCount = 0;
    int runCount = 0;
    int nextRepetitionSearchIndex = 0;
    qint64 scratchReserveGrowCount = 0;
    for (int spriteIndex = 0; spriteIndex < sprites.size(); ++spriteIndex) {
        const PreviewSpriteDescriptor& sprite = sprites[spriteIndex];
        if (sprite.image == nullptr
            || sprite.image->isNull()
            || sprite.opacity <= 0.0
            || sprite.width <= 0.0
            || sprite.height <= 0.0) {
            continue;
        }

        const auto repetition = spriteIndex >= nextRepetitionSearchIndex
            ? miacode::preview::quick_scene::previewSpriteRepetitionAt(sprites, spriteIndex)
            : miacode::preview::quick_scene::PreviewSpriteRepetition();
        if (repetition.count > 0) {
            if (runCount >= runs.size()) runs.append(OrderedRunScratch());
            if (buildRepeatedRun(sprites, spriteIndex, repetition, *root, atlasTexture, textures, &runs[runCount])) {
                visibleSpriteCount += runs[runCount].spriteCount;
                spriteIndex += runs[runCount].spriteCount - 1;
                runs[runCount].repetitionUpper = false;
                if (runCount + 1 >= runs.size()) runs.append(OrderedRunScratch());
                runs[runCount + 1] = runs[runCount];
                runs[runCount + 1].spriteCount = 0;
                runs[runCount + 1].repetitionUpper = true;
                runCount += 2;
                continue;
            }
            nextRepetitionSearchIndex = spriteIndex + repetition.period * repetition.count;
        }

        const auto atlasIt = sprite.cacheable ? root->atlasRects.constFind(sprite.image->cacheKey()) : root->atlasRects.constEnd();
        const bool usesAtlas = atlasTexture != nullptr && atlasIt != root->atlasRects.constEnd();
        QSGTexture* texture = usesAtlas
            ? atlasTexture : textures->textureForImage(*sprite.image, sprite.cacheable);
        if (texture == nullptr) {
            continue;
        }
        texture->setFiltering(QSGTexture::Linear);

        // Batch by the *resolved* texture pointer rather than the QImage cacheKey: the texture
        // repository now de-duplicates visually-identical QImage instances down to a single
        // QSGTexture, so any two sprites that the GPU can draw with the same atlas slot share
        // one familyKey here. Using image.cacheKey() instead would split such sprites into
        // separate runs even though they bind the same texture, defeating the dedup work.
        const SpriteFamilyKey familyKey = static_cast<SpriteFamilyKey>(reinterpret_cast<quintptr>(texture));
        if (runCount == 0 || runs.at(runCount - 1).repetition != nullptr || runs.at(runCount - 1).familyKey != familyKey) {
            if (runCount >= runs.size()) {
                runs.append(OrderedRunScratch());
            }
            OrderedRunScratch& run = runs[runCount];
            run.familyKey = familyKey;
            run.texture = texture;
            run.vertices.clear();
            run.spriteCount = 0;
            run.repetition.reset();
            run.repetitionUpper = false;
            const int initialReserveHint =
                miacode::preview::quick_scene::previewQuickSpriteInitialReserveHint(
                    root->familyVertexReserveHint,
                    familyKey);
            if (run.vertices.capacity() < initialReserveHint) {
                run.vertices.reserve(initialReserveHint);
                ++scratchReserveGrowCount;
            }
            ++runCount;
        }

        OrderedRunScratch& run = runs[runCount - 1];
        const int baseVertexIndex = run.spriteCount * 6;
        const int requiredVertexCount = baseVertexIndex + 6;
        if (requiredVertexCount > run.vertices.capacity()) {
            run.vertices.reserve(
                miacode::preview::quick_scene::previewQuickSpriteAlignedVertexCapacity(requiredVertexCount));
            ++scratchReserveGrowCount;
        }
        if (run.vertices.size() < requiredVertexCount) {
            run.vertices.resize(requiredVertexCount);
        }
        QRectF uvRect;
        if (usesAtlas) {
            const QRectF source = sprite.sourceRect.isValid() && !sprite.sourceRect.isEmpty()
                ? sprite.sourceRect : QRectF(QPointF(), sprite.image->size());
            uvRect = texture->convertToNormalizedSourceRect(source.translated(atlasIt->topLeft()));
        } else {
            uvRect = normalizedSourceRect(texture, sprite);
        }
        writeSpriteVertices(
            run.vertices.data(),
            baseVertexIndex,
            sprite,
            uvRect);
        run.spriteCount += 1;
        visibleSpriteCount += 1;
    }

    PreviewSpriteBatchFrameProfile frameProfile;
    frameProfile.spriteCount = visibleSpriteCount;
    frameProfile.batchCount = runCount;
    frameProfile.runCount = runCount;
    for (int index = 0; index < runCount; ++index) frameProfile.verticesRequired += runs[index].vertices.size();
    frameProfile.scratchReserveGrowCount = scratchReserveGrowCount;
    if (runCount == 0) {
        textures->noteSpriteBatchStats(profileLayerName, frameProfile);
        delete root;
        return nullptr;
    }

    const qreal wave = miacode::preview::scene::previewAnimatedSpriteWave(playheadSeconds);
    const qreal absWave = qAbs(wave);

    QVector<QSGNode*>& existingChildren = root->scratchChildren;
    existingChildren.clear();
    for (QSGNode* child = root->firstChild(); child != nullptr; child = child->nextSibling()) {
        existingChildren.append(child);
    }

    int nodeIndex = 0;
    for (int runIndex = 0; runIndex < runCount; ++runIndex) {
        OrderedRunScratch& run = runs[runIndex];
        QSGNode* existing = nodeIndex < existingChildren.size() ? existingChildren.at(nodeIndex) : nullptr;
        auto* batchNode = dynamic_cast<PreviewQuickSpriteBatchNode*>(existing);
        if (batchNode == nullptr || batchNode->isRepeated() != (run.repetition != nullptr)
            || batchNode->isUpper() != run.repetitionUpper) {
            auto* newNode = new PreviewQuickSpriteBatchNode(run.repetition != nullptr, run.repetitionUpper);
            if (existing != nullptr) {
                root->insertChildNodeBefore(newNode, existing);
                root->removeChildNode(existing);
                delete existing;
                existingChildren[nodeIndex] = newNode;
            } else {
                root->appendChildNode(newNode);
                existingChildren.append(newNode);
            }
            batchNode = newNode;
        }

        const PreviewQuickSpriteBatchNodeUpdateProfile batchProfile = batchNode->updateBatch(run, wave, absWave);
        frameProfile.geometryReallocCount += batchProfile.geometryReallocated ? 1 : 0;
        frameProfile.verticesCapacity += batchProfile.allocatedVertexCapacity;
        const int previousHint = root->familyVertexReserveHint.value(run.familyKey, 0);
        if (batchProfile.requiredVertexCount > previousHint) {
            root->familyVertexReserveHint.insert(run.familyKey, batchProfile.requiredVertexCount);
        }
        ++nodeIndex;
    }
    textures->noteSpriteBatchStats(profileLayerName, frameProfile);

    for (int index = existingChildren.size() - 1; index >= nodeIndex; --index) {
        QSGNode* child = existingChildren.at(index);
        root->removeChildNode(child);
        delete child;
    }
    return root;
}
