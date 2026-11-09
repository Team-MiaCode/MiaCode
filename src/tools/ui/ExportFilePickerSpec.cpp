#include "app/ui/export/ExportSession.h"
#include "app/ui/preview/PreviewSettingsModel.h"
#include "app/ui/media/MediaToolsModel.h"
#include "app/ui/dialogs/NativeFolderPicker.h"
#include "app/services/JobProgressService.h"
#include "app/services/PreferenceDocument.h"
#include "app/services/PreviewAppearanceState.h"
#include "app/services/ShellNotifications.h"
#include "app/services/UiRequestService.h"
#include "app/services/VideoExportPreferences.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScopedValueRollback>
#include <QSignalSpy>
#include <QHash>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QtTest/QTest>

#include <memory>

namespace {
class Engine final : public miacode::ExportEngine {
public:
    VideoExportTask task;
    VideoExportTask launchedTask;
    bool launchSucceeds = false;
    bool batchLaunchSucceeds = false;
    BatchResult batchResult;
    int seeds = 0;
    VideoExportTask buildSeedTask(int) override { ++seeds; return task; }
    QList<int> difficultyIds() const override { return {4, 5}; }
    QString difficultyChartText(int) const override { return {}; }
    int lastOpenedDifficultyId() const override { return 5; }
    void applySharedTaskSettings(const VideoExportTask&) override {}
    bool startAudition(int, const VideoExportTask&) override { return false; }
    void clearAudition() override {}
    void stopAudition() override {}
    bool launchVideoExport(const VideoExportTask& requested, int, QString*) override
    {
        launchedTask = requested;
        return launchSucceeds;
    }
    bool launchBatchExport(const VideoExportTask& requested, const QStringList&, const QList<int>&,
                          const QString&, BatchResult* result, const BatchCallbacks&, QString*) override
    {
        launchedTask = requested;
        *result = batchResult;
        return batchLaunchSucceeds;
    }
    void cancelVideoExport() override {}
    MuriRenderOptions muriRenderOptions() const override { return {}; }
    double currentAudioClockSecond() const override { return 0; }
    void refreshIntroState() override {}
    void setIntroAuditionPlaying(bool) override {}
    bool introAuditionPlaying() const override { return false; }
};
}

bool verifyVideoTemplates(const QString& workspace, QTextStream& err)
{
    const QString root = QFileInfo(workspace).canonicalFilePath();
    const auto withinWorkspace = [&root](const QString& directory) {
        const QString path = QFileInfo(directory).canonicalFilePath();
        return !root.isEmpty() && (path == root || path.startsWith(root + QLatin1Char('/')));
    };
    const QString temp = QDir::tempPath();
    const QString appDirectory = QCoreApplication::applicationDirPath();
    if (!withinWorkspace(temp) || !withinWorkspace(appDirectory)) {
        err << "Template fixture requires a workspace-local temp and binary directory" << Qt::endl;
        return false;
    }
    QTemporaryDir scratch(QDir(temp).filePath(QStringLiteral("video-templates-XXXXXX")));
    if (!scratch.isValid() || !withinWorkspace(scratch.path())) {
        err << "Unable to create template fixture directory" << Qt::endl;
        return false;
    }
    const QString preferencesPath = scratch.filePath(QStringLiteral("preferences.json"));
    QTextStream(stdout) << "Template fixture preferences: " << preferencesPath << Qt::endl;
    PreferenceDocument::Repository repository(preferencesPath, {});
    using miacode::app_preferences::VideoExportPreferences;
    const QScopedValueRollback preferences(miacode::app_preferences::videoExportPreferences(),
        VideoExportPreferences([&repository] { return repository.snapshot(); },
                               [&repository](const QJsonObject& value) { return repository.replace(value); }));
    // An explicit empty value keeps seedFromDifficulty away from the legacy native store.
    if (!miacode::app_preferences::saveDialogPreferences({{QStringLiteral("last_batch_output_directory"), QString()}})) {
        err << "Unable to seed isolated template preferences" << Qt::endl;
        return false;
    }
    QDir parent(scratch.path());
    miacode::UiRequestService historyRequests;
    miacode::ShellNotifications notifications;
    miacode::JobProgressService progress;
    miacode::PreviewAppearanceState appearance;
    miacode::PreviewSurface* preview = nullptr;
    QString historyId;
    QObject::connect(&historyRequests, &miacode::UiRequestService::fileRequested,
                     [&](const QString& id, const QVariantMap&) { historyId = id; });
    const auto wait = [] { QCoreApplication::processEvents(); QTest::qWait(20); };
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << Qt::endl; ok = false; }
    };
    Engine seed;
    seed.task.chartPath = parent.filePath(QStringLiteral("maidata.txt"));
    seed.task.outputPath = parent.filePath(QStringLiteral("original.mp4"));
    miacode::ExportEngine* seedSlot = &seed;
    {
        seed.task.contentDurationSeconds = 120.0;
        miacode::ui::ExportSession session(notifications, historyRequests, progress, appearance, seedSlot, preview);
        session.enter(5);
        wait();
        session.setFps(30);
        session.setAudioBitrateKbps(256);
        session.setIntroEnabled(true);
        session.saveVideoTemplate(QStringLiteral("Upload"));
        session.renameVideoTemplate(QStringLiteral("Upload"), QStringLiteral("Sharing"));
        expect(session.videoTemplates().size() == 1
                   && session.videoTemplates().constFirst().toMap().value(QStringLiteral("name")) == QStringLiteral("Sharing"),
               "a named video template can be saved and renamed");
        session.setFps(60);
        session.setAudioBitrateKbps(192);
        session.setIntroEnabled(false);
        session.setIntroPvPreview(true);
        session.setIntroPvStartSeconds(17.5);
        session.setExportRangeSeconds(5, 25);
        session.setOutputPath(parent.filePath(QStringLiteral("keep.mp4")));
        session.applyVideoTemplate(QStringLiteral("Sharing"));
        expect(session.fps() == 30 && session.audioBitrateKbps() == 256,
               "applying a video template restores encoding settings");
        expect(session.outputPath() == parent.filePath(QStringLiteral("keep.mp4"))
                   && session.exportStartSeconds() == 5 && session.exportEndSeconds() == 25
                   && !session.introEnabled() && session.introPvPreview() && session.introPvStartSeconds() == 17.5,
               "applying a template preserves the output, selected range and one-export intro selection");
        session.exportVideoTemplate();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(parent.filePath(QStringLiteral("copy.json.miatemplate"))));
        const QString templatePath = parent.filePath(QStringLiteral("copy.miatemplate"));
        QFile templateFile(templatePath);
        expect(templateFile.open(QIODevice::ReadOnly), "export writes one application suffix");
        QJsonObject document = QJsonDocument::fromJson(templateFile.readAll()).object();
        const QJsonObject settings = document.value(QStringLiteral("settings")).toObject();
        expect(document.value(QStringLiteral("kind")) == QStringLiteral("miacode-video-template")
                   && !settings.contains(QStringLiteral("intro_background_custom_path"))
                   && !settings.contains(QStringLiteral("add_intro"))
                   && !settings.contains(QStringLiteral("intro_style")),
               "the file stores portable preferences and omits one-export fields");
        templateFile.close();
        session.setFps(60);
        session.importVideoTemplate();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(templatePath));
        expect(session.fps() == 30 && session.videoTemplates().size() == 2,
               "import applies and saves a local copy of the template");
        const QString legacyTemplatePath = parent.filePath(QStringLiteral("legacy-video.json"));
        QFile oldTemplate(legacyTemplatePath);
        expect(oldTemplate.open(QIODevice::WriteOnly), "legacy template fixture opens");
        oldTemplate.write(QJsonDocument(document).toJson());
        oldTemplate.close();
        session.setFps(60);
        session.importVideoTemplate();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(legacyTemplatePath));
        expect(session.fps() == 30 && session.videoTemplates().size() == 3,
               "the same template document can be imported with a JSON suffix");
        session.removeVideoTemplate(QStringLiteral("Sharing"));
        expect(session.videoTemplates().size() == 2, "a saved template can be deleted");
        session.leave();
    }
    if (ok) {
        QTextStream(stdout) << "video templates passed" << Qt::endl;
    }
    return ok;
}

// ExportSession reads v1's batch history from the native QSettings store,
// which QSettings::setPath cannot redirect. Set the developer's real values
// aside for the run and put them back afterwards, even when a check fails.
class LegacyBatchHistoryGuard
{
public:
    LegacyBatchHistoryGuard()
    {
        for (const QString& key : keys()) {
            if (store_.contains(key)) saved_.insert(key, store_.value(key));
            store_.remove(key);
        }
    }
    ~LegacyBatchHistoryGuard()
    {
        for (const QString& key : keys()) {
            if (saved_.contains(key)) store_.setValue(key, saved_.value(key));
            else store_.remove(key);
        }
        store_.sync();
    }
    QString value(const QString& key) { store_.sync(); return store_.value(key).toString(); }
    void setValue(const QString& key, const QString& value) { store_.setValue(key, value); store_.sync(); }

private:
    static QStringList keys()
    {
        return {QStringLiteral("batch_video_export_dialog/last_chart_directory"),
                QStringLiteral("batch_video_export_dialog/last_output_directory")};
    }
    QSettings store_{QStringLiteral("fanfaredash"), QStringLiteral("MiaCode")};
    QHash<QString, QVariant> saved_;
};

int main(int argc, char** argv)
{
    const bool native = argc > 1 && QByteArray(argv[1]) == "--native";
    if (!native) qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    app.setApplicationName(native ? QStringLiteral("MiaCodeNativeFolderPickerSpec")
                                  : QStringLiteral("MiaCodeExportFilePickerSpec"));
    QTranslator translator;
    if (translator.load(QStringLiteral(":/i18n/zh_CN.qm"))) app.installTranslator(&translator);
    QTextStream err(stderr);
    const QStringList arguments = app.arguments();
    if (arguments.value(1) == QLatin1String("--templates-only")) {
        return verifyVideoTemplates(arguments.value(2), err) ? 0 : 1;
    }
    LegacyBatchHistoryGuard legacy;
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << Qt::endl; ok = false; }
    };
    QTemporaryDir scratch(QDir::temp().filePath(QStringLiteral("MiaCode picker #XXXXXX")));
    expect(scratch.isValid(), "scratch directory exists");
    QDir parent(scratch.path());
    parent.mkdir(QStringLiteral("Song #1"));
    parent.mkdir(QStringLiteral("中文 Song 2"));
    const QList<QUrl> selected{
        QUrl::fromLocalFile(parent.filePath(QStringLiteral("Song #1"))),
        QUrl::fromLocalFile(parent.filePath(QStringLiteral("中文 Song 2"))),
    };
    const QStringList expected{
        QFileInfo(selected.first().toLocalFile()).canonicalFilePath(),
        QFileInfo(selected.last().toLocalFile()).canonicalFilePath(),
    };
    PreferenceDocument::savePreferencesObject({});
    miacode::app_preferences::saveDialogPreferences({
        {QStringLiteral("last_batch_chart_directory"), scratch.path()},
        {QStringLiteral("unrelated_setting"), 42},
    });

    miacode::UiRequestService requests;
    miacode::UiRequestService::NativeFolderResult finish;
    QPointer<QObject> owner;
    QPointer<QObject> pickerLifetime;
    QUrl startFolder;
    QString pickerTitle;
    int opens = 0;
    requests.setNativeFolderPicker([&](QObject* lifetime, QObject* parentWindow, const QString& title,
                                      const QUrl& folder, auto callback) {
        pickerLifetime = lifetime;
        owner = parentWindow;
        startFolder = folder;
        pickerTitle = title;
        finish = std::move(callback);
        ++opens;
    });
    if (native) miacode::ui::installNativeFolderPicker(requests);
    miacode::ShellNotifications notifications;
    miacode::JobProgressService progress;
    miacode::PreviewAppearanceState appearance;
    miacode::ExportEngine* exportEngine = nullptr;
    miacode::PreviewSurface* preview = nullptr;
    QSignalSpy requested(&requests, &miacode::UiRequestService::fileRequested);
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    engine.rootContext()->setContextProperty(QStringLiteral("pickerRequests"), &requests);
    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import QtQuick.Controls
import MiaCode.UI
ApplicationWindow {
    width: 900; height: 700; visible: true
    title: "MiaCode native folder selection verification"
    UiRequestHost { requests: pickerRequests }
}
)QML", QUrl(QStringLiteral("qrc:/FilePickerSpec.qml")));
    std::unique_ptr<QObject> window(component.create());
    if (!window) { err << component.errorString(); return 1; }
    const auto wait = [&] { QCoreApplication::processEvents(); QTest::qWait(20); };
    if (native) {
        miacode::ui::ExportSession session(notifications, requests, progress, appearance, exportEngine, preview);
        int stage = 0;
        QTimer check;
        check.setInterval(200);
        QObject::connect(&check, &QTimer::timeout, &app, [&] {
            if (requests.pendingFileRequestCount() != 0) return;
            auto actual = session.chartDirectories();
            auto sortedExpected = expected;
            actual.sort();
            sortedExpected.sort();
            expect(actual == sortedExpected, "native picker returns both selected directories");
            if (!ok) { app.exit(1); return; }
            if (stage++ == 0) {
                QTextStream(stdout) << "native selection ok; reopening for cancellation" << Qt::endl;
                session.addChartDirectories();
            } else {
                QTextStream(stdout) << "native selection, remembered location and cancellation ok" << Qt::endl;
                app.exit(0);
            }
        });
        check.start();
        QTimer::singleShot(120000, &app, [&] { app.exit(2); });
        QTextStream(stdout) << "Native fixture: " << scratch.path() << Qt::endl;
        session.addChartDirectories();
        return app.exec();
    }
    {
        miacode::ui::ExportSession session(notifications, requests, progress, appearance, exportEngine, preview);
        session.addChartDirectories();
        wait();
        expect(requested.last().at(1).toMap().value("selectMultiple").toBool(), "batch export requests multiple folders");
        expect(opens == 1 && owner == window.get() && pickerLifetime,
               "the QML shell opens the system picker with the owning window and lifetime");
        expect(startFolder.toLocalFile() == scratch.path(), "native picker receives the persisted location");
        expect(pickerTitle == qtTrId("dialog.batch_export.select_charts"), "native picker receives the translated title");
        finish(selected, {});
        expect(!pickerLifetime && session.chartDirectories() == expected, "all native paths are added and the picker lifetime ends");
        session.addChartDirectories();
        wait();
        expect(startFolder.toLocalFile() == scratch.path(), "reopening starts at the selected folders' parent");
        finish(selected, {});
        expect(session.chartDirectories() == expected, "adding the same folders again does not duplicate jobs");
        session.clearChartDirectories();
        session.addChartDirectories();
        wait();
        finish({}, {});
        expect(session.chartDirectories().isEmpty(), "native cancellation does not add a folder");
    }
    PreferenceDocument::Repository disk(PreferenceDocument::preferencesFilePath());
    const auto stored = disk.snapshot().value("app").toObject().value("video_export").toObject();
    expect(stored.value("last_batch_chart_directory").toString() == scratch.path(), "parent location survives on disk and cancellation");
    expect(stored.value("unrelated_setting").toInt() == 42, "remembering a folder preserves other export settings");
    {
        miacode::ui::ExportSession reopened(notifications, requests, progress, appearance, exportEngine, preview);
        reopened.addChartDirectories();
        wait();
        expect(startFolder.toLocalFile() == scratch.path(), "new export sessions restore the location after clearing the queue");
        finish({}, {});
        auto prefs = miacode::app_preferences::loadDialogPreferences();
        prefs.insert("last_batch_chart_directory", parent.filePath("deleted"));
        miacode::app_preferences::saveDialogPreferences(prefs);
        reopened.addChartDirectories();
        wait();
        expect(QDir(startFolder.toLocalFile()).exists(), "deleted remembered folders fall back to an existing location");
        finish({}, {});
    }
    // Caller policy: exercise real models without opening a platform window.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch.path());
    miacode::UiRequestService historyRequests;
    QString historyId;
    QVariantMap historyRequest;
    QObject::connect(&historyRequests, &miacode::UiRequestService::fileRequested,
                     [&](const QString& id, const QVariantMap& value) { historyId = id; historyRequest = value; });
    const auto chosen = [&](const QString& name) {
        parent.mkpath(name);
        return parent.filePath(name);
    };
    const QString introSource = chosen("intro-source");
    const QString fontSource = chosen("font-source");
    const QString hudSource = chosen("hud-source");
    const QString pvRoot = chosen("pv-root");
    const auto folder = [&] { return historyRequest.value("startFolder").toUrl().toLocalFile(); };
    Engine seed;
    seed.task.chartPath = parent.filePath("Song #1/maidata.txt");
    seed.task.outputPath = QStringLiteral("test_MAS.mp4");
    miacode::ExportEngine* seedSlot = &seed;
    {
        miacode::ui::ExportSession session(notifications, historyRequests, progress, appearance, seedSlot, preview);
        session.enter(5);
        wait();
        session.browseOutputPath();
        expect(folder() == QFileInfo(seed.task.chartPath).absolutePath(), "relative video output starts beside the chart");
        historyRequests.cancelFileRequest(historyId);
        expect(session.batchOutputDirectory().isEmpty(), "batch output stays empty without an explicit choice");
        session.browseBatchOutputDirectory();
        expect(folder() == QDir::homePath() && session.batchOutputDirectory().isEmpty(),
               "opening the output picker does not default the output to the chart or working directory");
        historyRequests.cancelFileRequest(historyId);
        expect(session.batchOutputDirectory().isEmpty(), "cancelling leaves the output unset");
        session.browseBatchOutputDirectory();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(fontSource));
        expect(session.batchOutputDirectory() == fontSource, "an explicit output directory is retained");
        session.browseBatchOutputDirectory();
        expect(folder() == fontSource, "the output picker restores the explicit choice");
        historyRequests.cancelFileRequest(historyId);
        session.setBatchOutputDirectory({});
        session.leave();
        session.enter(5);
        wait();
        expect(session.batchOutputDirectory().isEmpty(), "a cleared output remains empty after reseeding");
        session.importIntroSound();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(introSource + "/sound.wav"));
        session.importIntroFont();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(fontSource + "/font.ttf"));
    }
    {
        miacode::ui::ExportSession session(notifications, historyRequests, progress, appearance, seedSlot, preview);
        session.importIntroSound();
        expect(folder() == introSource, "intro sound restores its source after session recreation");
        historyRequests.cancelFileRequest(historyId);
        session.importIntroFont();
        expect(folder() == fontSource, "intro font has an independent source directory");
        historyRequests.cancelFileRequest(historyId);
    }
    {
        miacode::ui::PreviewSettingsModel settings(notifications, historyRequests, appearance, preview);
        settings.importHudFont();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(hudSource + "/hud.ttf"));
    }
    {
        miacode::ui::PreviewSettingsModel settings(notifications, historyRequests, appearance, preview);
        settings.importHudFont();
        expect(folder() == hudSource, "HUD font restores its own source after model recreation");
        historyRequests.cancelFileRequest(historyId);
    }
    miacode::MediaToolsEngine* media = nullptr;
    {
        miacode::ui::MediaToolsModel tools(historyRequests, progress, media);
        tools.chooseBatchDirectory();
        historyRequests.submitFileResult(historyId, QUrl::fromLocalFile(pvRoot));
    }
    {
        miacode::ui::MediaToolsModel tools(historyRequests, progress, media);
        tools.chooseBatchDirectory();
        expect(folder() == pvRoot, "PV root survives model recreation");
        historyRequests.cancelFileRequest(historyId);
        expect(tools.batchDirectory() == pvRoot, "cancelling PV selection preserves the root");
    }
    const QString legacyRoot = chosen("legacy-root");
    const QString legacyChart = chosen("legacy-root/song");
    legacy.setValue(QStringLiteral("batch_video_export_dialog/last_chart_directory"), legacyChart);
    auto withoutNewHistory = miacode::app_preferences::loadDialogPreferences();
    withoutNewHistory.remove("last_batch_chart_directory");
    miacode::app_preferences::saveDialogPreferences(withoutNewHistory);
    {
        miacode::ui::ExportSession session(notifications, historyRequests, progress, appearance, seedSlot, preview);
        session.addChartDirectories();
        expect(folder() == legacyRoot, "v1 chart-directory history opens its parent on upgrade");
        historyRequests.cancelFileRequest(historyId);
        expect(miacode::app_preferences::loadDialogPreferences().value("last_batch_chart_directory").toString().isEmpty(),
               "cancelling an upgrade browse does not create a new selection history");
        session.addChartDirectories();
        historyRequests.submitFileResults(historyId, {QUrl::fromLocalFile(legacyChart)});
        expect(miacode::app_preferences::loadDialogPreferences().value("last_batch_chart_directory").toString() == legacyRoot,
               "an accepted upgraded selection persists the new parent policy");
        auto current = miacode::app_preferences::loadDialogPreferences();
        current.insert("last_batch_chart_directory", fontSource);
        miacode::app_preferences::saveDialogPreferences(current);
        session.addChartDirectories();
        expect(folder() == fontSource, "new batch input history wins over the old preference");
        historyRequests.cancelFileRequest(historyId);
        current.insert("last_batch_chart_directory", parent.filePath("deleted-current"));
        miacode::app_preferences::saveDialogPreferences(current);
        session.addChartDirectories();
        expect(folder() != legacyRoot && QDir(folder()).exists(), "deleted new history falls back without resurrecting old history");
        historyRequests.cancelFileRequest(historyId);
    }
    legacy.setValue(QStringLiteral("batch_video_export_dialog/last_output_directory"), legacyRoot);
    auto withoutOutputHistory = miacode::app_preferences::loadDialogPreferences();
    withoutOutputHistory.remove("last_batch_output_directory");
    miacode::app_preferences::saveDialogPreferences(withoutOutputHistory);
    {
        miacode::ui::ExportSession session(notifications, historyRequests, progress, appearance, seedSlot, preview);
        session.enter(5);
        wait();
        expect(session.batchOutputDirectory() == legacyRoot, "v1 batch output history survives the upgrade");
        session.setBatchOutputDirectory(fontSource);
        expect(miacode::app_preferences::loadDialogPreferences().value("last_batch_output_directory").toString() == fontSource,
               "a new batch output choice is recorded in the dialog preferences");
        expect(legacy.value(QStringLiteral("batch_video_export_dialog/last_output_directory")) == legacyRoot,
               "v1's native settings store is only read, never written");
        session.setBatchOutputDirectory({});
        session.leave();
        session.enter(5);
        wait();
        expect(session.batchOutputDirectory().isEmpty(), "a cleared output does not revive v1's history");
    }
    {
        auto preferences = miacode::app_preferences::loadDialogPreferences();
        preferences.insert("add_intro", false);
        preferences.insert("intro_style", "pv_preview");
        preferences.insert("intro_pv_brightness", 0.35);
        miacode::app_preferences::saveDialogPreferences(preferences);
        seed.task.contentDurationSeconds = 120.0;
        miacode::ui::ExportSession session(notifications, historyRequests, progress, appearance, seedSlot, preview);
        session.enter(5);
        wait();
        expect(session.introEnabled() && !session.introPvPreview()
                   && session.introPvStartSeconds() == 0.0,
               "new export defaults to enabled classic intro and clears the PV range");
        const auto selectIntro = [&] {
            session.setIntroEnabled(true);
            session.setIntroPvPreview(true);
            session.setIntroPvStartSeconds(42.5);
        };
        selectIntro();
        session.selectDifficulty(4);
        expect(session.introEnabled() && session.introPvPreview() && session.introPvStartSeconds() == 42.5,
               "changing difficulty preserves the pending intro selection");
        session.startExport();
        expect(session.introEnabled() && session.introPvPreview() && session.introPvStartSeconds() == 42.5,
               "launch failure preserves the intro selection for retry");
        seed.launchSucceeds = true;
        session.startExport();
        expect(seed.launchedTask.intro.enabled && seed.launchedTask.intro.pvPreview
                   && seed.launchedTask.intro.pvPreviewStartSeconds == 42.5,
               "export receives the selected intro and PV segment");
        emit notifications.videoExportWorkerRunningChanged(false);
        expect(session.introEnabled() && session.introPvStartSeconds() == 42.5,
               "cancelled or failed worker preserves the intro selection");
        session.startExport();
        emit notifications.videoExportCompleted();
        emit notifications.videoExportWorkerRunningChanged(false);
        expect(session.introEnabled() && !session.introPvPreview() && session.introPvStartSeconds() == 0.0,
               "successful export enables classic intro and resets PV mode and range");
        expect(qAbs(session.introPvBrightness() - 0.35) < 1e-6,
               "successful export retains PV brightness");
        session.startExport();
        expect(seed.launchedTask.intro.enabled && !seed.launchedTask.intro.pvPreview
                   && seed.launchedTask.intro.pvPreviewStartSeconds == 0.0,
               "next export defaults to classic intro and requires a new PV selection");
        emit notifications.videoExportWorkerRunningChanged(false);
        session.setIntroEnabled(false);
        session.startExport();
        expect(!seed.launchedTask.intro.enabled, "intro can still be disabled for the current export");
        emit notifications.videoExportCompleted();
        emit notifications.videoExportWorkerRunningChanged(false);
        expect(session.introEnabled(), "successful export re-enables intro even when it was manually disabled");
        session.setActiveTab("batch");
        seed.batchLaunchSucceeds = true;
        seed.batchResult.canceled = true;
        selectIntro();
        session.startExport();
        expect(session.introEnabled() && session.introPvStartSeconds() == 42.5,
               "batch cancellation before any success preserves intro selection");
        seed.batchResult.canceled = false;
        seed.batchResult.successCount = 2;
        session.startExport();
        expect(seed.launchedTask.intro.pvPreviewStartSeconds == 42.5
                   && session.introEnabled() && !session.introPvPreview()
                   && session.introPvStartSeconds() == 0.0,
               "batch uses the chosen segment and resets intro selection after completion");
        const auto saved = miacode::app_preferences::loadDialogPreferences();
        expect(!saved.contains("add_intro") && !saved.contains("intro_style"),
               "export does not persist intro enablement or PV mode");
    }
    miacode::FileRequest save;
    save.saveMode = true;
    save.startPath = parent.filePath(QStringLiteral("out #2.mp4"));
    QString savedPath;
    requests.requestFile(save, [&](const QString& path) { savedPath = path; });
    wait();
    auto* fileDialog = window->findChild<QObject*>(QStringLiteral("uiRequestFileDialog"));
    expect(fileDialog && fileDialog->property("selectedFile").toUrl().toLocalFile() == save.startPath,
           "single-file save dialogs still receive the proposed local file URL");
    if (fileDialog) {
        QMetaObject::invokeMethod(fileDialog, "accepted");
        QMetaObject::invokeMethod(fileDialog, "close");
    }
    expect(savedPath == save.startPath && requests.pendingFileRequestCount() == 0,
           "single-file results retain their existing callback contract");
    if (ok) QTextStream(stdout) << "export_file_picker_spec ok" << Qt::endl;
    return ok ? 0 : 1;
}
