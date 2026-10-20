#include "export/session/PreviewQuickExportSession.h"
#include "preview/runtime/PreviewSceneAssetRepository.h"
#include "core/scene/PreviewSceneConstants.h"
#include "core/scene/PreviewProgressStatsCache.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTextStream>
#include <QtQml/qqmlextensionplugin.h>

#include <memory>

Q_IMPORT_QML_PLUGIN(MiaCode_PreviewPlugin)

int main(int argc, char** argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"background", "Background image.", "path"});
    parser.addOption({"skin", "Note skin directory; outline uses Line.", "path"});
    parser.addOption({"output", "Frame output directory.", "path"});
    parser.addOption({"size", "Square side in pixels.", "pixels", "720"});
    parser.addOption({"fps", "Frames per second.", "fps", "60"});
    parser.addOption({"times", "Comma-separated frame times in seconds.", "seconds"});
    parser.process(app);
    const int side = parser.value("size").toInt();
    const int fps = parser.value("fps").toInt();
    const QString output = parser.value("output");
    const QImage background(parser.value("background"));
    if (background.isNull() || side <= 0 || fps <= 0 || output.isEmpty()
        || !QDir().mkpath(output)) return 1;

    using namespace miacode::preview::scene;
    miacode::preview::runtime::PreviewSceneAssetRepository assets;
    assets.setOutlineVariant(PreviewOutlineVariant::Line);
    assets.setStageMediaAvailable(true);
    if (!assets.loadSkinDirectorySync(parser.value("skin"))) return 1;
    PreviewFrameState state;
    state.assets = assets.assetState();
    state.skin = assets.skinAssets();
    state.judgeOverlay = assets.judgeOverlayAssets();
    state.judgeEffect = assets.judgeEffectAssets();
    state.media.stageMediaAvailable = true;
    state.media.resolvedStageImage = background;
    state.media.resolvedStageImageCacheable = true;
    state.media.stageMediaSerial = 1;
    state.render.backgroundBrightnessOuter = 0.0;
    state.render.backgroundBrightnessInner = 0.4;
    state.render.showTimestamp = false;
    state.render.showObjectStatsHud = false;
    state.render.showChartInfoHud = false;
    state.render.centerDisplayMode = miacode::preview_gameplay::CenterDisplayMode::Off;
    TimelineNoteMarker marker;
    marker.type = QStringLiteral("touch");
    marker.second = 0.5;
    marker.endSecond = marker.second;
    marker.isFirework = true;
    marker.touchPoint = QPointF(kLogicalCanvasCenter, kLogicalCanvasCenter);
    state.noteMarkers.append(marker);
    auto stats = std::make_shared<PreviewProgressStatsCache>();
    stats->rebuild(state.noteMarkers);
    state.progressStatsCache = stats;
    state.sceneContentRevision = 1;

    // Composite the background and effects in the production GPU export session.
    // A single opaque readback retains emission and avoids overlay alpha conversion.
    PreviewQuickExportSession renderer;
    renderer.setLayerFlags(StageBackgroundLayer | BackdropLayer | JudgeFireworkLayer);
    renderer.setFrameSize(QSize(side, side));
    renderer.setFrameState(state);
    QString error;
    if (!renderer.initialize(format, nullptr, &error)) {
        QTextStream(stderr) << error << Qt::endl;
        return 1;
    }
    QList<double> frameTimes;
    if (parser.isSet("times")) {
        for (const QString& value : parser.value("times").split(',')) {
            bool valid = false;
            const double second = value.toDouble(&valid);
            if (!valid) return 1;
            frameTimes.append(second);
        }
    } else {
        for (int frame = 0; frame < 2 * fps; ++frame) {
            frameTimes.append(double(frame) / fps);
        }
    }
    for (qsizetype frame = 0; frame < frameTimes.size(); ++frame) {
        renderer.applyExportFrameTick(frameTimes[frame], false, false, true, 0, fps);
        const QImage image = renderer.renderFrame(&error);
        if (image.isNull()) {
            QTextStream(stderr) << error << Qt::endl;
            return 1;
        }
        if (!image.save(QDir(output).filePath(QStringLiteral("%1.png").arg(frame, 4, 10, QLatin1Char('0'))))) return 1;
    }
    QTextStream(stdout) << output << Qt::endl;
    return 0;
}
