#include "tools/cover_export/CoverCompositeRenderer.h"

#include "common/ChartAssetPaths.h"
#include "common/IntroAssetImages.h"
#include "tools/cover_export/CoverLayoutModel.h"
#include "preview/quick_scene/PreviewQuickSceneRoot.h"
#ifdef MIACODE_MOBILE
#include "preview/runtime/PreviewQuickExportSession.h"
#endif

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QUrl>
#include <Qt>
#include <QtQml>
#include <memory>

Q_LOGGING_CATEGORY(coverCompositeLog, "miacode.cover.composite", QtWarningMsg)

namespace miacode::cover_export {
namespace {

constexpr char kComposerQmlUrl[] = "qrc:/intro/qml/CoverComposer.qml";
constexpr char kCoverChartImageProviderId[] = "coverchart";

void ensureComposerQmlTypesRegistered()
{
    static const bool registered = [] {
        if (qmlTypeId("MiaCode.Preview", 1, 0, "PreviewQuickSceneRoot") < 0)
            qmlRegisterType<PreviewQuickSceneRoot>("MiaCode.Preview", 1, 0, "PreviewQuickSceneRoot");
        return true;
    }();
    Q_UNUSED(registered);
}

class CoverChartImageProvider final : public QQuickImageProvider
{
public:
    explicit CoverChartImageProvider(CoverLayoutModel* model)
        : QQuickImageProvider(QQuickImageProvider::Image)
        , model_(model)
    {
    }

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        Q_UNUSED(requestedSize);
        QString key = id;
        const int queryPos = key.indexOf(QLatin1Char('?'));
        if (queryPos >= 0) {
            key.truncate(queryPos);
        }
        QImage image;
        if (model_ != nullptr) {
            if (CoverLayer* layer = model_->layer(key)) {
                image = layer->frameImage();
            }
        }
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32);
            image.fill(Qt::transparent);
        }
        if (size != nullptr) {
            *size = image.size();
        }
        return image;
    }

private:
    QPointer<CoverLayoutModel> model_;
};

void settleEvents(bool cold)
{
    if (cold) {
        for (int i = 0; i < 12; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(3);
        }
        return;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
}

QVariantMap composerProperties(CoverLayoutModel* model,
                               const CoverComposerInputs& inputs, bool editable)
{
    return {
        {QStringLiteral("coverLayout"), QVariant::fromValue<QObject*>(model)},
        {QStringLiteral("coverTemplate"), inputs.templateMap},
        {QStringLiteral("trackOverrides"), inputs.trackOverrides},
        {QStringLiteral("jacketImage"), miacode::chart_assets::displayBackgroundImageUrl(inputs.jacketPath)},
        {QStringLiteral("backgroundImage"), miacode::chart_assets::displayBackgroundImageUrl(inputs.backgroundPath)},
        {QStringLiteral("backgroundMode"), static_cast<int>(inputs.backgroundMode)},
        {QStringLiteral("blurEnabled"), inputs.blurBackground},
        {QStringLiteral("coverBgBrightness"), inputs.coverBgBrightness},
        {QStringLiteral("cardShadowEnabled"), inputs.cardShadow},
        {QStringLiteral("chartFrameBgEnabled"), inputs.chartFrameBackground},
        {QStringLiteral("chartFrameBgTransparency"), inputs.chartFrameBgTransparency},
        {QStringLiteral("chartFrameBgBrightness"), inputs.chartFrameBgBrightness},
        {QStringLiteral("chartFrameDiskDiameter"), inputs.chartFrameDiskDiameter},
        {QStringLiteral("activeChartFrameKey"), QString()},
        {QStringLiteral("editable"), editable},
    };
}

void applyComposerInputs(QQuickItem* root, CoverLayoutModel* model,
                         const CoverComposerInputs& inputs, bool editable)
{
    if (!root) return;
    const auto properties = composerProperties(model, inputs, editable);
    for (auto it = properties.cbegin(); it != properties.cend(); ++it)
        root->setProperty(it.key().toUtf8().constData(), it.value());
}

QString uniqueCoverPath(const QDir& dir, const QString& extension, const QString& baseName)
{
    // Only a file name is accepted, even when a caller supplies chart metadata.
    QString name = baseName;
    for (auto& ch : name) {
        if (ch.unicode() < 32 || QStringLiteral("<>:\"/\\|?*").contains(ch)) ch = QChar('_');
    }
    name = name.trimmed().left(120);
    // Android providers commonly enforce a byte limit per file name. Leave
    // room for collision suffixes and the extension with CJK chart titles.
    while (name.toUtf8().size() > 180) name.chop(1);
    if (!name.isEmpty() && name.back().isHighSurrogate()) name.chop(1);
    while (name.endsWith('.') || name.endsWith(' ')) name.chop(1);
    if (name.isEmpty()) name = QStringLiteral("card");
    QString candidate = dir.filePath(name + '.' + extension);
    int copyIndex = 1;
    while (QFileInfo::exists(candidate)) {
        candidate = dir.filePath(QStringLiteral("%1(%2).%3").arg(name).arg(copyIndex).arg(extension));
        ++copyIndex;
    }
    return QFileInfo(candidate).absoluteFilePath();
}

}  // namespace

void registerCoverChartImageProvider(QQmlEngine* engine, CoverLayoutModel* model)
{
    if (engine == nullptr) {
        return;
    }
    miacode::intro::registerIntroAssetImages(engine);
    engine->addImageProvider(QString::fromLatin1(kCoverChartImageProviderId),
                             new CoverChartImageProvider(model));
}

QImage renderCoverComposite(CoverLayoutModel* model,
                            const CoverComposerInputs& inputs,
                            const QSize& fullSize,
                            QString* errorMessage, PreviewQuickExportSession* reusableRenderer)
{
    const int width = qMax(1, fullSize.width());
    const int height = qMax(1, fullSize.height());
    const bool transparent = inputs.backgroundMode == CoverBackgroundMode::Transparent;
    if (inputs.templateMap.isEmpty() || !inputs.templateMap.contains(QStringLiteral("card"))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("banner template missing or invalid (no \"card\" entry)");
        }
        return QImage();
    }

    ensureComposerQmlTypesRegistered();
#ifdef MIACODE_MOBILE
    std::unique_ptr<PreviewQuickExportSession> ownedRenderer;
    if (!reusableRenderer) ownedRenderer = std::make_unique<PreviewQuickExportSession>();
    auto& renderer = reusableRenderer ? *reusableRenderer : *ownedRenderer;
    renderer.setFrameSize(QSize(width, height));
    qCDebug(coverCompositeLog) << "initialize begin";
    if (!renderer.initialize(QSurfaceFormat(), nullptr, errorMessage)) return {};
    qCDebug(coverCompositeLog) << "composition begin";
    if (!renderer.setupComposition(QUrl(QString::fromLatin1(kComposerQmlUrl)),
            composerProperties(model, inputs, false),
            [model](QQmlEngine* engine) { registerCoverChartImageProvider(engine, model); }, errorMessage))
        return {};
    qCDebug(coverCompositeLog) << "composition ready";
    // Local images and fonts load synchronously. Drive the composition's own
    // polish/render directly: pumping the application's event loop here can
    // enter an Android window-surface wait during Home or lock transitions.
    // Queue cancellation and publication are handled between bounded captures.
    QImage image;
    for (int i = 0; i < 3; ++i) {
        qCDebug(coverCompositeLog) << "render begin" << i;
        image = renderer.renderFrame(errorMessage);
        qCDebug(coverCompositeLog) << "render returned" << i << !image.isNull();
        if (image.isNull()) return {};
    }
#else
    Q_UNUSED(reusableRenderer);
    auto* window = new QQuickWindow();
    window->setFlags(window->flags() | Qt::FramelessWindowHint | Qt::Tool
                     | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    window->setColor(transparent ? QColor(Qt::transparent) : QColor(Qt::black));
    window->resize(width, height);
    window->setPosition(-32000, -32000);

    auto* engine = new QQmlEngine(window);
    registerCoverChartImageProvider(engine, model);
    QQmlComponent component(engine, QUrl(QString::fromLatin1(kComposerQmlUrl)));
    if (component.isError()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("failed to load CoverComposer.qml: %1")
                                .arg(component.errorString().trimmed());
        }
        delete window;
        return QImage();
    }
    QObject* object = component.create(engine->rootContext());
    auto* root = qobject_cast<QQuickItem*>(object);
    if (root == nullptr) {
        delete object;
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("CoverComposer.qml root is not a QQuickItem");
        }
        delete window;
        return QImage();
    }
    root->setParentItem(window->contentItem());
    root->setParent(window->contentItem());
    root->setSize(QSizeF(width, height));
    applyComposerInputs(root, model, inputs, false);

    window->setOpacity(0.0);
    window->show();
    settleEvents(true);
    QImage image = window->grabWindow();
    if (image.isNull()) {
        settleEvents(true);
        image = window->grabWindow();
    }
    delete window;
#endif
    if (image.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("grabWindow returned an empty image");
        }
        return QImage();
    }
    image.setDevicePixelRatio(1.0);
    if (image.size() != QSize(width, height)) {
        image = image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return image.convertToFormat(transparent ? QImage::Format_ARGB32 : QImage::Format_RGB32);
}

CoverExportResult exportCoverComposite(CoverLayoutModel* model,
                                       const CoverComposerInputs& inputs,
                                       const QSize& fullSize,
                                       const QString& outputDirectory, const QString& baseName,
                                       PreviewQuickExportSession* reusableRenderer)
{
    CoverExportResult result;
    QDir outputDir(outputDirectory);
    if (outputDirectory.trimmed().isEmpty()
        || (!outputDir.exists() && !outputDir.mkpath(QStringLiteral(".")))) {
        result.errorMessage = QStringLiteral("invalid output directory: %1").arg(outputDirectory);
        return result;
    }
    QImage image = renderCoverComposite(model, inputs, fullSize, &result.errorMessage, reusableRenderer);
    if (image.isNull()) {
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = QStringLiteral("failed to render the cover");
        }
        return result;
    }
    const bool transparent = inputs.backgroundMode == CoverBackgroundMode::Transparent;
    const QString outputPath = uniqueCoverPath(outputDir, transparent ? QStringLiteral("png")
                                                                    : QStringLiteral("jpg"), baseName);
    if (!image.save(outputPath, transparent ? "PNG" : "JPG", transparent ? -1 : 95)) {
        result.errorMessage = QStringLiteral("failed to write image: %1").arg(outputPath);
        return result;
    }
    result.success = true;
    result.outputPath = outputPath;
    return result;
}

}  // namespace miacode::cover_export
