// Manual diagnostic: render a difficulty-card cover with one chart-frame layer
// through the production cover compositor, without the cover editor UI.
//
//   miacode_cover_render --chart <dir|maidata.txt> --seconds 60
//       [--difficulty 5] [--frame-bg pv|image|transparent] [--size 1920x1080]
//       [--skin <skin dir>] [--card-shadow] [--card-only] --output cover.png
//
// The chart task comes from the export snapshot builder (the worker's path),
// the still from SceneFrameRenderer and the PV frame from CoverPvFrameSource,
// so the image matches what the cover export writes for the same layout.

#include "core/chart/ChartAssetPaths.h"
#include "core/chart/document/SimaiDocument.h"
#include "export/cover_export/CoverCompositeRenderer.h"
#include "export/cover_export/CoverLayoutModel.h"
#include "export/cover_export/CoverPvFrameSource.h"
#include "export/cover_export/SceneFrameRenderer.h"
#include "export/video_export/VideoExportSnapshot.h"

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(MiaCode_PreviewPlugin)

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "miacode_cover_render: " << message << Qt::endl;
    return 1;
}

QString chartFilePath(const QString& input)
{
    const QFileInfo info(input);
    if (info.isDir()) {
        return QDir(input).filePath(QStringLiteral("maidata.txt"));
    }
    return info.absoluteFilePath();
}

QVariantMap bannerTemplate()
{
    QFile file(QStringLiteral(":/intro/templates/maimai_banner.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap{};
}

QString difficultyAtlasKey(int difficultyId)
{
    switch (difficultyId) {
    case 2: return QStringLiteral("BASIC");
    case 3: return QStringLiteral("ADVANCED");
    case 4: return QStringLiteral("EXPERT");
    case 6: return QStringLiteral("ReMASTER");
    default: return QStringLiteral("MASTER");
    }
}

}  // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("chart"), QStringLiteral("Chart folder or maidata.txt."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("difficulty"), QStringLiteral("Difficulty id."), QStringLiteral("id"), QStringLiteral("5")});
    parser.addOption({QStringLiteral("seconds"), QStringLiteral("Chart second of the frame."), QStringLiteral("seconds"), QStringLiteral("0")});
    parser.addOption({QStringLiteral("frame-bg"), QStringLiteral("Chart-frame background: pv, image or transparent."), QStringLiteral("mode"), QStringLiteral("pv")});
    parser.addOption({QStringLiteral("size"), QStringLiteral("Cover size WxH."), QStringLiteral("size"), QStringLiteral("1920x1080")});
    parser.addOption({QStringLiteral("skin"), QStringLiteral("Skin directory."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("output"), QStringLiteral("Output .png/.jpg."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("card-shadow"), QStringLiteral("Draw the difficulty card's shadow.")});
    parser.addOption({QStringLiteral("card-only"),
                      QStringLiteral("Only the centred card, on a transparent background (PNG).")});
    parser.process(app);

    const QString chartPath = chartFilePath(parser.value(QStringLiteral("chart")));
    QFile chartFile(chartPath);
    if (!chartFile.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("cannot read chart %1").arg(chartPath));
    }
    const QString chartText = QString::fromUtf8(chartFile.readAll());
    const int difficultyId = parser.value(QStringLiteral("difficulty")).toInt();
    const double seconds = qMax(0.0, parser.value(QStringLiteral("seconds")).toDouble());
    const QString frameBgMode = parser.value(QStringLiteral("frame-bg"));
    const QStringList sizeParts = parser.value(QStringLiteral("size")).split(QLatin1Char('x'));
    const QSize size(sizeParts.value(0).toInt(), sizeParts.value(1).toInt());
    const QString skinDirectory = parser.isSet(QStringLiteral("skin"))
        ? parser.value(QStringLiteral("skin"))
        : QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("assets/skin/skinSD"));
    const QString outputPath = parser.value(QStringLiteral("output"));
    if (size.isEmpty() || outputPath.isEmpty()) {
        return fail(QStringLiteral("--size and --output are required"));
    }

    VideoExportSnapshot snapshot;
    snapshot.chartTextUtf8 = chartText;
    snapshot.difficultyId = difficultyId;
    snapshot.originalChartPath = chartPath;
    snapshot.skinDirectory = skinDirectory;
    snapshot.contentDurationSeconds = 1.0;
    VideoExportTask task;
    QString error;
    if (!buildVideoExportTaskFromSnapshot(snapshot, &task, &error)) {
        return fail(error);
    }
    task.contentDurationSeconds = qMax(seconds + 1.0, task.contentDurationSeconds);

    const int side = qBound(512, qMax(size.width(), size.height()), 4096);
    miacode::cover_export::SceneFrameRenderer renderer;
    if (!renderer.bootstrap(task, &error)
        || !renderer.prepareCaptureWindow(side, seconds, &error)) {
        return fail(error);
    }
    QElapsedTimer wait;
    wait.start();
    while (!renderer.captureReady() && wait.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    const QImage still = renderer.renderAt(seconds, side, &error);
    if (still.isNull()) {
        return fail(QStringLiteral("chart still: %1").arg(error));
    }

    miacode::cover_export::CoverLayoutModel model;
    model.ensureDefaultLayers();
    miacode::cover_export::CoverLayer* frame = model.addChartFrameLayer(seconds);
    frame->setNx(0.64);
    frame->setNy(0.5);
    frame->setSizeFraction(0.94);
    frame->setFrameBgMode(frameBgMode);
    frame->setFrameBgBrightness(0.8);
    model.setLayerImage(frame->key(), still);
    if (miacode::cover_export::CoverLayer* card = model.layer(miacode::cover_export::CoverLayoutModel::cardKey())) {
        card->setNx(0.25);
        card->setNy(0.5);
        card->setSizeFraction(0.78);
        card->setZ(frame->z() + 1);
        if (parser.isSet(QStringLiteral("card-only"))) {
            frame->setVisible(false);
            card->setNx(0.5);
            card->setSizeFraction(0.9);
        }
    }

    if (frame->frameBgMode() == QStringLiteral("pv")) {
        if (!miacode::chart_assets::isVideoBackgroundPath(task.backgroundMediaPath)) {
            return fail(QStringLiteral("chart has no video PV (%1)").arg(task.backgroundMediaPath));
        }
        QElapsedTimer decodeTimer;
        decodeTimer.start();
        const QImage pvFrame = miacode::cover_export::CoverPvFrameSource::decodeFrame(
            task.backgroundMediaPath, seconds, side, &error);
        if (pvFrame.isNull()) {
            return fail(error);
        }
        QTextStream(stdout) << "pv_frame " << pvFrame.width() << "x" << pvFrame.height()
                            << " decodeMs=" << decodeTimer.elapsed() << Qt::endl;
        model.setLayerPvFrame(frame->key(), pvFrame);
    }

    const SimaiDocument document = SimaiDocument::fromText(chartText);
    const SimaiDifficultyData* difficulty = document.difficulty(difficultyId);
    miacode::cover_export::CoverComposerInputs inputs;
    inputs.templateMap = bannerTemplate();
    inputs.trackOverrides = {
        {QStringLiteral("title"), document.title},
        {QStringLiteral("artist"), document.artist},
        {QStringLiteral("designer"), difficulty != nullptr && !difficulty->designer.isEmpty()
                                         ? difficulty->designer : document.designer},
        {QStringLiteral("level"), difficulty != nullptr ? difficulty->level : QString()},
        {QStringLiteral("difficulty"), difficultyAtlasKey(difficultyId)},
        {QStringLiteral("bpm"), QString::number(task.clockBpm, 'f', 0)},
        {QStringLiteral("mode"), QStringLiteral("DX")},
        {QStringLiteral("lvRenderMode"), QStringLiteral("atlas")},
        {QStringLiteral("stillTextMode"), QStringLiteral("shrink")},
    };
    inputs.jacketPath = miacode::chart_assets::resolveDisplayBackgroundImagePath(chartPath);
    inputs.cardShadow = parser.isSet(QStringLiteral("card-shadow"));
    if (parser.isSet(QStringLiteral("card-only"))) {
        inputs.backgroundMode = miacode::cover_export::CoverBackgroundMode::Transparent;
    }
    inputs.chartFrameBackground = frame->frameBgMode() == QStringLiteral("image");
    inputs.chartFrameBgBrightness = frame->frameBgBrightness();
    inputs.chartFrameBgTransparency = frame->frameBgTransparency();
    inputs.chartFrameDiskDiameter = renderer.playfieldDiskDiameterFraction();

    const auto result = miacode::cover_export::exportCoverComposite(&model, inputs, size, outputPath);
    if (!result.success) {
        return fail(result.errorMessage);
    }
    QTextStream(stdout) << "cover " << QDir::toNativeSeparators(result.outputPath) << Qt::endl;
    return 0;
}
