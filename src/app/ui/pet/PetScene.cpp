#include "app/ui/pet/PetScene.h"

#include <QFile>
#include <QImage>
#include <QMatrix4x4>
#include <rhi/qrhi.h>
#include <cstddef>
#include <memory>

namespace miacode::ui {
namespace {

struct Vertex {
    float x, y, z;
    float u, v;
};

QShader loadShader(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QShader::fromSerialized(file.readAll());
}

void appendMesh(const PetModelNode* node, const QMatrix4x4& parent, QVector<Vertex>& vertices)
{
    if (!node || !node->visible)
        return;
    QMatrix4x4 transform = parent;
    transform.translate(node->position);
    transform.rotate(node->rotation);
    transform.rotate(QQuaternion::fromEulerAngles(node->eulerRotation));
    transform.scale(node->scale);
    if (const auto* mesh = node->geometry) {
        for (int index : mesh->indexes) {
            const QVector3D point = transform.map(mesh->positions[index]);
            const QVector2D uv = mesh->uv0s[index];
            vertices.append({point.x(), point.y(), point.z(), uv.x(), uv.y()});
        }
    }
    for (QObject* child : node->childNodes()) {
        if (auto* childNode = qobject_cast<PetModelNode*>(child))
            appendMesh(childNode, transform, vertices);
    }
}

class PetSceneRenderer final : public QQuickRhiItemRenderer {
public:
    void synchronize(QQuickRhiItem* item) override
    {
        const auto* scene = static_cast<PetScene*>(item);
        vertices_.clear();
        appendMesh(scene->model(), QMatrix4x4(), vertices_);
        cameraMirrored_ = scene->cameraMirrored();
    }

    void initialize(QRhiCommandBuffer*) override
    {
        if (rhi_ != rhi()) {
            pipeline_.reset();
            bindings_.reset();
            sampler_.reset();
            texture_.reset();
            uniforms_.reset();
            verticesBuffer_.reset();
            vertexCapacity_ = 0;
            rhi_ = rhi();
        }
        if (pipeline_)
            return;

        uniforms_.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        uniforms_->create();
        skin_ = QImage(QStringLiteral(":/qt/qml/MiaCode/UI/pet/minecraft/fox.png"))
                    .convertToFormat(QImage::Format_RGBA8888);
        texture_.reset(rhi_->newTexture(QRhiTexture::RGBA8, skin_.size()));
        texture_->create();
        sampler_.reset(rhi_->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                       QRhiSampler::None, QRhiSampler::ClampToEdge,
                                       QRhiSampler::ClampToEdge));
        sampler_->create();
        bindings_.reset(rhi_->newShaderResourceBindings());
        bindings_->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                    uniforms_.get()),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                     texture_.get(), sampler_.get())
        });
        bindings_->create();

        pipeline_.reset(rhi_->newGraphicsPipeline());
        pipeline_->setShaderStages({
            {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/src/app/ui/pet/shaders/pet.vert.qsb"))},
            {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/src/app/ui/pet/shaders/pet.frag.qsb"))}
        });
        QRhiVertexInputLayout layout;
        layout.setBindings({{sizeof(Vertex)}});
        layout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float3, offsetof(Vertex, x)},
            {0, 1, QRhiVertexInputAttribute::Float2, offsetof(Vertex, u)}
        });
        pipeline_->setVertexInputLayout(layout);
        pipeline_->setDepthTest(true);
        pipeline_->setDepthWrite(true);
        pipeline_->setCullMode(QRhiGraphicsPipeline::Back);
        pipeline_->setSampleCount(renderTarget()->sampleCount());
        pipeline_->setShaderResourceBindings(bindings_.get());
        pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        pipeline_->create();
    }

    void render(QRhiCommandBuffer* cb) override
    {
        auto* updates = rhi_->nextResourceUpdateBatch();
        if (!skin_.isNull()) {
            updates->uploadTexture(texture_.get(), skin_);
            skin_ = {};
        }
        const quint32 bytes = quint32(vertices_.size() * sizeof(Vertex));
        if (bytes > vertexCapacity_) {
            vertexCapacity_ = bytes;
            verticesBuffer_.reset(rhi_->newBuffer(QRhiBuffer::Dynamic,
                                                 QRhiBuffer::VertexBuffer, vertexCapacity_));
            verticesBuffer_->create();
        }
        if (bytes)
            updates->updateDynamicBuffer(verticesBuffer_.get(), 0, bytes, vertices_.constData());

        const QSize size = renderTarget()->pixelSize();
        QMatrix4x4 projection;
        projection.perspective(34.0f, float(size.width()) / float(size.height()), 0.01f, 100.0f);
        QMatrix4x4 view;
        view.lookAt({cameraMirrored_ ? 1.7f : -1.7f, 1.05f, -2.1f}, {0, 0.5f, 0}, {0, 1, 0});
        const QMatrix4x4 matrix = rhi_->clipSpaceCorrMatrix() * projection * view;
        updates->updateDynamicBuffer(uniforms_.get(), 0, 64, matrix.constData());

        cb->beginPass(renderTarget(), Qt::transparent, {1.0f, 0}, updates);
        if (bytes) {
            cb->setGraphicsPipeline(pipeline_.get());
            cb->setViewport({0, 0, float(size.width()), float(size.height())});
            cb->setShaderResources();
            const QRhiCommandBuffer::VertexInput input(verticesBuffer_.get(), 0);
            cb->setVertexInput(0, 1, &input);
            cb->draw(quint32(vertices_.size()));
        }
        cb->endPass();
    }
private:
    QRhi* rhi_ = nullptr;
    QVector<Vertex> vertices_;
    quint32 vertexCapacity_ = 0;
    bool cameraMirrored_ = false;
    QImage skin_;
    std::unique_ptr<QRhiBuffer> verticesBuffer_;
    std::unique_ptr<QRhiBuffer> uniforms_;
    std::unique_ptr<QRhiTexture> texture_;
    std::unique_ptr<QRhiSampler> sampler_;
    std::unique_ptr<QRhiShaderResourceBindings> bindings_;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline_;
};

} // namespace

PetScene::PetScene(QQuickItem* parent) : QQuickRhiItem(parent)
{
    setAlphaBlending(true);
}

void PetScene::setModel(PetModelNode* model)
{
    if (model_ == model)
        return;
    model_ = model;
    emit modelChanged();
    update();
}

void PetScene::setCameraMirrored(bool value)
{
    if (cameraMirrored_ == value)
        return;
    cameraMirrored_ = value;
    emit cameraMirroredChanged();
    update();
}

QQuickRhiItemRenderer* PetScene::createRenderer()
{
    return new PetSceneRenderer;
}

} // namespace miacode::ui
