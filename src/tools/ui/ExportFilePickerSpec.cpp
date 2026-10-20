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
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
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
    QSettings legacy(QStringLiteral("fanfaredash"), QStringLiteral("MiaCode"));
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
