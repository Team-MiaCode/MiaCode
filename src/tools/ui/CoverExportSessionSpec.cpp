#include "app/ui/export/CoverExportSession.h"
#include "app/services/PreferenceDocument.h"
#include "app/services/ProjectPreferences.h"
#include "common/DebugLog.h"
#include "export/cover_export/SceneFrameRenderer.h"
#include "core/scene/PreviewPreparedSceneCache.h"

#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
class Engine final : public miacode::ExportEngine {
public:
    VideoExportTask task;
    int seeds = 0;
    VideoExportTask buildSeedTask(int) override { ++seeds; return task; }
    QList<int> difficultyIds() const override { return {4, 5}; }
    QString difficultyChartText(int) const override { return {}; }
    int lastOpenedDifficultyId() const override { return 5; }
    void applySharedTaskSettings(const VideoExportTask&) override {}
    bool startAudition(int, const VideoExportTask&) override { return false; }
    void clearAudition() override {}
    void stopAudition() override {}
    bool launchVideoExport(const VideoExportTask&, int, QString*) override { return false; }
    bool launchBatchExport(const VideoExportTask&, const QStringList&, const QList<int>&,
                          const QString&, BatchResult*, const BatchCallbacks&, QString*) override { return false; }
    void cancelVideoExport() override {}
    MuriRenderOptions muriRenderOptions() const override { return {}; }
    double currentAudioClockSecond() const override { return 0; }
    void refreshIntroState() override {}
    void setIntroAuditionPlaying(bool) override {}
    bool introAuditionPlaying() const override { return false; }
};
}

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MiaCodeCoverExportSessionSpec"));
    QTemporaryDir temporary;
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << Qt::endl; ok = false; }
    };
    PreferenceDocument::savePreferencesObject({});
    QDir directory(temporary.path());
    directory.mkdir(QStringLiteral("a"));
    directory.mkdir(QStringLiteral("b"));
    const QString chartA = temporary.filePath(QStringLiteral("a/maidata.txt"));
    const QString chartB = temporary.filePath(QStringLiteral("b/maidata.txt"));
    const QString custom = temporary.filePath(QStringLiteral("a/custom.png"));
    QImage red(16, 16, QImage::Format_RGB32);
    red.fill(Qt::red);
    expect(red.save(custom), "fixture image saved");

    Engine engine;
    engine.task.chartPath = chartA;
    engine.task.intro.jacketPath = temporary.filePath(QStringLiteral("a/bg.png"));
    miacode::UiRequestService requests;
    miacode::PlaybackControl* playback = nullptr;
    QString requestId;
    QVariantMap request;
    QObject::connect(&requests, &miacode::UiRequestService::fileRequested,
                     [&](const QString& id, const QVariantMap& value) { requestId = id; request = value; });
    {
        miacode::ui::CoverExportSession session(engine, requests, playback);
        session.enter(5);
        expect(session.outputFilePath() == temporary.filePath(QStringLiteral("a/card.jpg")),
               "default output is inside chart A");
        session.setOutputFile(QStringLiteral("covers/front.png"));
        session.browseBackgroundImage();
        requests.submitFileResult(requestId, QUrl::fromLocalFile(custom));
        expect(session.backgroundImage() == QUrl::fromLocalFile(custom), "custom background applied to A");
        session.browseOutputFile();
        const QString oldOutputRequest = requestId;
        engine.task.chartPath = chartB;
        engine.task.intro.jacketPath = temporary.filePath(QStringLiteral("b/bg.png"));
        session.refreshDocument(5);
        expect(session.outputFilePath() == temporary.filePath(QStringLiteral("b/card.jpg")),
               "chart B does not inherit A's destination before the debounce fires");
        expect(session.backgroundMode() == 0 && session.backgroundImage().isEmpty(),
               "chart B does not inherit A's custom background");
        expect(session.jacketImage() == QUrl::fromLocalFile(engine.task.intro.jacketPath),
               "chart B uses its own jacket");
        requests.submitFileResult(oldOutputRequest, QUrl::fromLocalFile(temporary.filePath("a/late.png")));
        expect(session.outputFile() == QStringLiteral("card.jpg"), "old file picker cannot overwrite B's destination");
        const auto savedA = miacode::project_preferences::load(chartA).value("coverExport").toObject();
        expect(savedA.value("outputFile") == QStringLiteral("covers/front.png")
                   && savedA.value("background").toObject().value("customPath") == custom,
               "A's pending destination and background are flushed to A");
        session.browseBackgroundImage();
        const QString oldBackgroundRequest = requestId;
        engine.task.chartPath = chartA;
        session.refreshDocument(5);
        expect(session.outputFilePath() == temporary.filePath(QStringLiteral("a/covers/front.png"))
                   && session.backgroundImage() == QUrl::fromLocalFile(custom),
               "returning to A restores its own destination and custom background");
        requests.submitFileResult(oldBackgroundRequest, QUrl::fromLocalFile(temporary.filePath("b/late.png")));
        expect(session.backgroundImage() == QUrl::fromLocalFile(custom), "old background picker cannot overwrite A");
        const int seedsBefore = engine.seeds;
        session.leave();
        session.enter(5);
        expect(engine.seeds == seedsBefore + 1, "reentering the same difficulty refreshes the source");
    }

    directory.mkpath("images"); directory.mkpath("fonts"); directory.mkpath("layouts");
    const QString imageSource = temporary.filePath("images/custom.png");
    expect(red.save(imageSource), "external image fixture saved");
    {
        miacode::ui::CoverExportSession session(engine, requests, playback);
        session.enter(5);
        session.addImageLayer();
        requests.submitFileResult(requestId, QUrl::fromLocalFile(imageSource));
        session.addImageLayer();
        expect(request.value("startFolder").toUrl().toLocalFile() == temporary.filePath("images"),
               "new cover image layers restore the external source folder");
        requests.cancelFileRequest(requestId);
        session.importCardDisplayFont();
        requests.submitFileResult(requestId, QUrl::fromLocalFile(temporary.filePath("fonts/test.ttf")));
        session.importCardBodyFont();
        expect(request.value("startFolder").toUrl().toLocalFile() == temporary.filePath("fonts"),
               "cover font inputs share their source folder");
        requests.cancelFileRequest(requestId);
        session.saveLayout();
        requests.submitFileResult(requestId, QUrl::fromLocalFile(temporary.filePath("layouts/test.miacover")));
    }
    {
        miacode::ui::CoverExportSession session(engine, requests, playback);
        session.enter(5);
        session.saveLayout();
        expect(request.value("startFolder").toUrl().toLocalFile() == temporary.filePath("layouts"),
               "cover layout save location survives session recreation");
        requests.cancelFileRequest(requestId);
        session.importLayout();
        expect(request.value("startFolder").toUrl().toLocalFile() == temporary.filePath("layouts"),
               "cover layout import shares the saved layout directory");
        requests.cancelFileRequest(requestId);
    }

    // The persistent live scene must rebuild its prepared windows on every new chart.
    miacode::cover_export::SceneFrameRenderer renderer;
    VideoExportTask task;
    task.skinDirectory = QStringLiteral(MIACODE_SOURCE_ROOT "/assets/skin/skinDX");
    TimelineNoteMarker marker;
    marker.type = QStringLiteral("tap");
    marker.lane = 1;
    marker.second = 1;
    task.noteMarkers = {marker};
    expect(renderer.bootstrap(task), "first renderer bootstrap succeeds");
    miacode::preview::scene::PreviewPreparedSceneCache cache;
    expect(cache.sync(*renderer.frameState()), "first chart builds prepared cache");
    const quint64 revision = renderer.frameState()->sceneContentRevision;
    marker.second = 8;
    task.noteMarkers = {marker, marker};
    expect(renderer.bootstrap(task) && renderer.frameState()->sceneContentRevision > revision
               && cache.sync(*renderer.frameState()) && cache.headLayer().entries.size() == 2,
           "rebootstrap replaces the previous chart's prepared notes");
    miacode::debug_log::shutdownAsyncLogWriter();
    return ok ? 0 : 1;
}
