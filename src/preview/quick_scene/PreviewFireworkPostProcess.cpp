#include "preview/quick_scene/PreviewFireworkPostProcess.h"

#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <QtMath>
#include <cstring>
#include <functional>

namespace {
struct Vertex {
    float x, y, u, v;
    float r, g, b, a;
};
const QSGGeometry::AttributeSet& attributes()
{
    static const QSGGeometry::Attribute a[] = {
        QSGGeometry::Attribute::create(0, 2, QSGGeometry::FloatType, true),
        QSGGeometry::Attribute::create(1, 2, QSGGeometry::FloatType),
        QSGGeometry::Attribute::create(2, 4, QSGGeometry::FloatType)
    };
    static const QSGGeometry::AttributeSet set = {3, sizeof(Vertex), a};
    return set;
}
class Material;
class Shader final : public QSGMaterialShader
{
public:
    explicit Shader(bool stars)
    {
        setShaderFileName(VertexStage, QStringLiteral(":/src/preview/quick_scene/shaders/PreviewFireworkPost.vert.qsb"));
        setShaderFileName(FragmentStage, stars
            ? QStringLiteral(":/src/preview/quick_scene/shaders/PreviewFireworkStar.frag.qsb")
            : QStringLiteral(":/src/preview/quick_scene/shaders/PreviewFireworkPost.frag.qsb"));
    }
    bool updateUniformData(RenderState&, QSGMaterial*, QSGMaterial*) override;
    void updateSampledImage(RenderState&, int, QSGTexture**, QSGMaterial*, QSGMaterial*) override;
};
class Material final : public QSGMaterial
{
public:
    explicit Material(bool isStars = false) : stars(isStars)
    {
        setFlag(Blending);
        setFlag(NoBatching);
    }
    QSGMaterialType* type() const override
    {
        static QSGMaterialType types[2];
        return &types[stars ? 1 : 0];
    }
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override
    {
        return new Shader(stars);
    }
    int compare(const QSGMaterial* other) const override
    {
        if (this == other) return 0;
        return std::less<const QSGMaterial*>{}(this, other) ? -1 : 1;
    }
    bool stars;
    QSGTexture* source = nullptr;
    QSGTexture* bloom = nullptr;
    float dx = 0.0f;
    float dy = 0.0f;
    float pass = 0.0f;
    std::array<float, 4> edgeMask{};
};
bool Shader::updateUniformData(RenderState& state, QSGMaterial* value, QSGMaterial*)
{
    auto* m = static_cast<Material*>(value);
    QByteArray* data = state.uniformData();
    if (state.isMatrixDirty()) {
        std::memcpy(data->data(), state.combinedMatrix().constData(), 64);
    }
    const float parameters[] = {state.opacity(), m->dx, m->dy, m->pass};
    std::memcpy(data->data() + 64, parameters, sizeof(parameters));
    std::memcpy(data->data() + 80, m->edgeMask.data(), sizeof(float) * 4);
    return true;
}
void Shader::updateSampledImage(RenderState& state, int binding, QSGTexture** texture, QSGMaterial* value, QSGMaterial*)
{
    auto* m = static_cast<Material*>(value);
    *texture = binding == 1 ? m->source : m->bloom;
    if (*texture) (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
}
class Quad final : public QSGGeometryNode
{
public:
    explicit Quad(bool stars = false) : geometry(attributes(), stars ? kFireworkStarCount * 6 : 6), material(stars)
    {
        geometry.setDrawingMode(QSGGeometry::DrawTriangles);
        geometry.setVertexDataPattern(QSGGeometry::DynamicPattern);
        setGeometry(&geometry);
        setMaterial(&material);
    }
    void setRect(const QRectF& r)
    {
        if (r == rect) return;
        rect = r;
        writeQuad(reinterpret_cast<Vertex*>(geometry.vertexData()), r, 1, 1, 1, 1);
        geometry.markVertexDataDirty();
        markDirty(DirtyGeometry);
    }
    static void writeQuad(Vertex* v, const QRectF& r, float red, float green, float blue, float alpha)
    {
        const float x = r.x(), y = r.y(), right = r.right(), bottom = r.bottom();
        v[0] = {x, y, 0, 0, red, green, blue, alpha};
        v[1] = {right, y, 1, 0, red, green, blue, alpha};
        v[2] = {x, bottom, 0, 1, red, green, blue, alpha};
        v[3] = v[1];
        v[4] = {right, bottom, 1, 1, red, green, blue, alpha};
        v[5] = v[2];
    }
    QSGGeometry geometry;
    Material material;
    QRectF rect;
};
QSGLayer* makeLayer(QQuickWindow* window, QSGNode* root)
{
    auto* context = QQuickWindowPrivate::get(window)->context;
    auto* layer = context->sceneGraphContext()->createLayer(context);
    layer->setItem(root);
    layer->setLive(false);
    layer->setRecursive(false);
    layer->setFormat(QSGLayer::RGBA8);
    layer->setHasMipmaps(false);
    layer->setMirrorVertical(true);
    layer->setFiltering(QSGTexture::Linear);
    layer->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    layer->setVerticalWrapMode(QSGTexture::ClampToEdge);
    return layer;
}
}
struct PreviewFireworkPostProcess::Data
{
    QQuickWindow* window;
    QSGRootNode sourceRoot;
    QSGRootNode starsRoot;
    QSGRootNode horizontalRoot;
    QSGRootNode verticalRoot;
    Quad* stars = new Quad(true);
    Quad* horizontal = new Quad;
    Quad* vertical = new Quad;
    Quad* output = new Quad;
    QSGLayer* starsLayer;
    QSGLayer* sourceLayer;
    QSGLayer* horizontalLayer;
    QSGLayer* verticalLayer;
    explicit Data(QQuickWindow* w) : window(w)
    {
        starsRoot.appendChildNode(stars);
        horizontalRoot.appendChildNode(horizontal);
        verticalRoot.appendChildNode(vertical);
        starsLayer = makeLayer(w, &starsRoot);
        sourceLayer = makeLayer(w, &sourceRoot);
        horizontalLayer = makeLayer(w, &horizontalRoot);
        verticalLayer = makeLayer(w, &verticalRoot);
        horizontal->material.source = sourceLayer;
        horizontal->material.bloom = sourceLayer;
        horizontal->material.pass = 1;
        vertical->material.source = horizontalLayer;
        vertical->material.bloom = horizontalLayer;
        vertical->material.pass = 2;
        output->material.source = sourceLayer;
        output->material.bloom = verticalLayer;
    }
    ~Data()
    {
        delete verticalLayer;
        delete horizontalLayer;
        delete sourceLayer;
        delete starsLayer;
    }
};
PreviewFireworkPostProcess::PreviewFireworkPostProcess(QQuickWindow* window) : d(new Data(window))
{
    appendChildNode(d->output);
    setFlag(UsePreprocess);
}
PreviewFireworkPostProcess::~PreviewFireworkPostProcess() { delete d; }
QSGNode* PreviewFireworkPostProcess::sourceRoot() const { return &d->sourceRoot; }
QSGTexture* PreviewFireworkPostProcess::starTexture() const { return d->starsLayer; }
void PreviewFireworkPostProcess::update(const QRectF& sourceBounds, const QRectF& stageBounds, float clipRadius, float exitScale, float smoothBlendStart,
    const std::array<FireworkStarState, kFireworkStarCount>& stars)
{
    const QRectF rect = sourceBounds.intersected(stageBounds);
    const qreal dpr = d->window->effectiveDevicePixelRatio();
    const QSize full(qMax(1, qCeil(rect.width() * dpr)), qMax(1, qCeil(rect.height() * dpr)));
    const QSize half(qMax(1, (full.width() + 1) / 2), qMax(1, (full.height() + 1) / 2));
    for (auto* layer : {d->starsLayer, d->sourceLayer, d->horizontalLayer, d->verticalLayer}) {
        layer->setRect(rect);
        layer->setSize(layer == d->starsLayer || layer == d->sourceLayer ? full : half);
        layer->markDirtyTexture();
        layer->scheduleUpdate();
    }
    for (auto* quad : {d->horizontal, d->vertical, d->output}) {
        quad->setRect(rect);
        quad->material.dx = clipRadius * 0.012f / rect.width();
        quad->material.dy = clipRadius * 0.012f / rect.height();
        quad->markDirty(DirtyMaterial);
    }
    d->output->material.edgeMask = {float(stageBounds.center().x()), float(stageBounds.center().y()),
        smoothBlendStart, smoothBlendStart > 0.0f ? clipRadius : 0.0f};
    static const float palette[5][3] = {{1,82.f/255,20.f/255},{245.f/255,72.f/255,148.f/255},{0,162.f/255,242.f/255},{186.f/255,242.f/255,0},{1,216.f/255,0}};
    auto* vertices = reinterpret_cast<Vertex*>(d->stars->geometry.vertexData());
    int count = 0;
    for (int slot = 0; slot < kFireworkStarCount; ++slot) {
        const auto& star = stars[slot];
        if (star.alpha <= 0 || star.radius <= 0) continue;
        const int index = slot % 12, batch = slot / 12;
        const float radius = star.radius * (.66f + float((index + batch * 2) % 5) * .035f) * exitScale;
        const auto& tint = palette[(index * 2 + batch + 1) % 5];
        const QPointF center = sourceBounds.center() + QPointF(star.x, star.y) * exitScale;
        Quad::writeQuad(vertices + count * 6, QRectF(center.x()-radius,center.y()-radius,2*radius,2*radius),
            .76f + .24f*tint[0], .76f*.98f+.24f*tint[1], .76f*.78f+.24f*tint[2], star.alpha*.95f);
        ++count;
    }
    // Keep the allocation stable; unused quads have zero area and coverage.
    std::memset(vertices + count*6, 0, sizeof(Vertex)*(kFireworkStarCount-count)*6);
    d->stars->geometry.markVertexDataDirty();
    d->stars->markDirty(DirtyGeometry);
}
void PreviewFireworkPostProcess::preprocess()
{
    d->starsLayer->updateTexture();
    d->sourceLayer->updateTexture();
    d->horizontalLayer->updateTexture();
    d->verticalLayer->updateTexture();
}
