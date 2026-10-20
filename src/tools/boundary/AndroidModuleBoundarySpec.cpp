// Android library combination: the Web set plus timeline, timeline_quick and
// audio_bass link and run without MiaCode, the editor, QtAVPlayer, the stage
// media host or the export pipeline. The target links only those libraries,
// so a hidden dependency on anything else fails to link here.

#include "audio/PreviewAudioWorkerFactory.h"
#include "audio/bass/BassPreviewAudioBackend.h"
#include "audio/bass/OfflineAudioDecoder.h"
#include "core/chart/parser/SimaiParser.h"
#include "preview/runtime/PreviewRuntime.h"
#include "timeline/TimelineQuickModel.h"
#include "timeline/quick/TimelineQuickStateBridge.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QTextStream>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(MiaCode_PreviewPlugin)
Q_IMPORT_QML_PLUGIN(MiaCode_TimelinePlugin)

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
    component.setData(source, QUrl(QStringLiteral("qrc:/boundary/AndroidModuleBoundary.qml")));
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

    const QString chart = QStringLiteral("(120){4}1,2,3,4,E");
    const SimaiParseResult parsed = SimaiParser::parseForTimeline(chart);
    expect(parsed.ok && parsed.noteMarkers.size() == 4, "chart: the parser produces the note model", out);

    // timeline: the model builds from chart text.
    TimelineQuickModel model;
    expect(model.rebuildFromText(chart, 0.0), "timeline: the timeline model builds from chart text", out);

    // timeline_quick: state bridge and the MiaCode.Timeline QML module.
    {
        TimelineQuickStateBridge bridge;
        Q_UNUSED(bridge);
    }
    QQmlEngine engine;
    expect(qmlTypeLoads(engine, "import QtQuick\nimport MiaCode.Timeline\nTimelineQuickItem {}\n"),
           "timeline_quick: MiaCode.Timeline 1.0 provides TimelineQuickItem", out);
    expect(qmlTypeLoads(engine, "import QtQuick\nimport MiaCode.Preview\nPreviewQuickSceneRoot {}\n"),
           "preview_quick: MiaCode.Preview 1.0 provides PreviewQuickSceneRoot", out);
    {
        PreviewRuntime runtime;
        runtime.setNoteMarkers(parsed.noteMarkers);
    }

    // audio_bass: the host installs BASS as the backend and decoder.
    expect(!miacode::preview_audio::productionPreviewAudioBackendFactory(),
           "audio: no backend before the host installs one", out);
    miacode::preview_audio::installPreviewAudioBackendProvider(
        miacode::preview_audio::bassPreviewAudioBackendProvider());
    expect(static_cast<bool>(miacode::preview_audio::productionPreviewAudioBackendFactory()),
           "audio_bass: the BASS provider becomes the production backend factory", out);
    const auto decoder = miacode::audio_decode::bassAudioFileDecoder();
    expect(decoder != nullptr && decoder->decoderName() == QStringLiteral("bass"),
           "audio_bass: the BASS decoder is available for injection", out);
    miacode::preview_audio::installPreviewAudioBackendProvider({});

    if (failures != 0) {
        out << "AndroidModuleBoundary spec failed: " << failures << Qt::endl;
        return 1;
    }
    out << "AndroidModuleBoundary spec passed." << Qt::endl;
    return 0;
}
