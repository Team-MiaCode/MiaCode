// Web library combination: base, chart, analysis, scene and
// preview_quick link and run without MiaCode, BASS, QtAVPlayer, the stage
// media host or the export pipeline. The target links only those libraries,
// so a hidden dependency on anything else fails to link here.

#include "common/PreferenceProvider.h"
#include "core/analysis/MuriAnalyzer.h"
#include "core/chart/parser/SimaiParser.h"
#include "core/scene/PreviewHudState.h"
#include "core/video/AssetPaths.h"
#include "preview/runtime/PreviewRuntime.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QTextStream>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(MiaCode_PreviewPlugin)

namespace {

int failures = 0;

void expect(bool condition, const char* label, QTextStream& out)
{
    out << (condition ? "[PASS] " : "[FAIL] ") << label << Qt::endl;
    if (!condition) {
        ++failures;
    }
}

bool qmlTypeLoads(QQmlEngine& engine, const QByteArray& source)
{
    QQmlComponent component(&engine);
    component.setData(source, QUrl(QStringLiteral("qrc:/boundary/WebModuleBoundary.qml")));
    QScopedPointer<QObject> object(component.create());
    if (!object) {
        QTextStream(stderr) << component.errorString() << Qt::endl;
    }
    return !object.isNull();
}

}  // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);

    // base: no provider is installed in a host that keeps no preferences.
    expect(miacode::preferences::preferenceProvider() == nullptr
               && miacode::preferences::resolvedLanguageToken().isEmpty(),
           "base: language overrides come only from an installed provider", out);

    // chart: parser and its slide reference data resource.
    expect(QFile::exists(QStringLiteral(":/data/slide_data.json")),
           "chart: :/data/slide_data.json is registered by the static library", out);
    const SimaiParseResult parsed = SimaiParser::parseForTimeline(QStringLiteral("(120){4}1,2,3,4,E"));
    expect(parsed.ok && parsed.noteMarkers.size() == 4, "chart: the parser produces the note model", out);

    // analysis: Muri runs on the chart model.
    const MuriAnalysisReport report = MuriAnalyzer::analyze(parsed.noteMarkers);
    expect(report.diagnostics.isEmpty(), "analysis: Muri analyzes the note model", out);

    // scene: scene math and the HUD font resource.
    const miacode::preview::scene::PreviewHudStats stats =
        miacode::preview::scene::computePreviewHudStats(parsed.noteMarkers, 10.0);
    expect(stats.tapTotal == 4 && stats.tapPlayed == 4, "scene: HUD statistics follow the chart", out);
    expect(QFile::exists(QStringLiteral(":/fonts/maple_mono_cn.ttf")),
           "scene: :/fonts/maple_mono_cn.ttf is registered by the static library", out);

    // The host can change or reset the resource root even after desktop
    // discovery has been cached. Invalid relative input must leave it intact.
    const QString discovered = miacode::assets::findAssetRoot();
    expect(miacode::assets::setAssetRoot(QStringLiteral("/browser/assets"))
               && miacode::assets::outlineLinePath() == QStringLiteral("/browser/assets/background/outline_line.png"),
           "assets: a browser host overrides cached desktop discovery", out);
    expect(!miacode::assets::setAssetRoot(QStringLiteral("relative/assets"))
               && miacode::assets::findAssetRoot() == QStringLiteral("/browser/assets"),
           "assets: relative roots are rejected without changing the host root", out);
    expect(miacode::assets::setAssetRoot(QStringLiteral(":/assets"))
               && miacode::assets::outlineLinePath() == QStringLiteral(":/assets/background/outline_line.png"),
           "assets: Qt resource roots preserve their URI", out);
    expect(miacode::assets::setAssetRoot(QString())
               && miacode::assets::findAssetRoot() == discovered,
           "assets: clearing the override restores desktop discovery", out);

    // preview_quick: runtime, resources and the MiaCode.Preview QML module.
    {
        PreviewRuntime runtime;
        runtime.setNoteMarkers(parsed.noteMarkers);
    }
    expect(QFile::exists(QStringLiteral(":/preview/judge_effects/judge_effect_tap.png")),
           "preview_quick: judge effect images are registered by the static library", out);
    expect(QFile::exists(QStringLiteral(":/src/preview/quick_scene/shaders/PreviewSpriteMaterial.frag.qsb")),
           "preview_quick: compiled sprite shaders are registered by the static library", out);
    QQmlEngine engine;
    expect(qmlTypeLoads(engine, "import QtQuick\nimport MiaCode.Preview\nItem {\n"
                                "    PreviewQuickSceneRoot {}\n    PreviewQuickHudLayer {}\n}\n"),
           "preview_quick: MiaCode.Preview 1.0 provides PreviewQuickSceneRoot and PreviewQuickHudLayer", out);

    if (failures != 0) {
        out << "WebModuleBoundary spec failed: " << failures << Qt::endl;
        return 1;
    }
    out << "WebModuleBoundary spec passed." << Qt::endl;
    return 0;
}
