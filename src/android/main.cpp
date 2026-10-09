#include "AndroidDocumentSession.h"
#include "AndroidEditorTools.h"
#include "app/ui/editor/EditorController.h"
#include "app/services/EditorSyncController.h"
#include "MobilePreview.h"
#include "MobileTimeline.h"
#include "MobileLatency.h"
#include "app/ui/latency/LatencyModel.h"
#include "EditorSyncUiSmoke.h"
#include "SettingsUiSmoke.h"
#include "PreferencesSmoke.h"
#include "SfxPlaybackSmoke.h"
#include "BackgroundUiSmoke.h"
#include "WorkbenchUiSmoke.h"
#include "LayoutUiSmoke.h"
#include "MobilePreferencesStore.h"
#include "PreferencesDialogUiSmoke.h"
#include "app/ui/preferences/PreferencesModel.h"
#include "app/ui/preferences/LocaleService.h"
#include "app/services/update/UpdateService.h"
#include "app/services/update/NetworkUpdateFetcher.h"
#include "app/services/update/PreferenceUpdateStateStore.h"
#include "AppVersion.h"
#include "MobileVideoExport.h"
#include "MobileExportComposition.h"
#include "MobileCoverComposition.h"
#ifdef Q_OS_ANDROID
#include "MobileWindowLifecycle.h"
#endif
#include "MobileZipExport.h"
#include "app/ui/preferences/AppBackgroundModel.h"
#include "app/ui/layout/WorkbenchSettings.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include "tools/cover_export/CoverCompositeRenderer.h"
#include "tools/video_export/VideoExportSettings.h"
#include "AndroidFileRequests.h"
#include "app/ui/document/AnalysisModel.h"
#include "app/ui/chrome/ShortcutModel.h"
#include "timeline/quick/TimelineQuickItem.h"
#include "preview/quick_scene/PreviewQuickSceneRoot.h"
#include "preview/quick_scene/PreviewQuickHudLayer.h"
#include "timeline/TimelineNoteAssets.h"
#include "common/AssetPaths.h"
#include "common/IntroAssetImages.h"
#include <QQuickImageProvider>
#include <QTranslator>
#ifdef Q_OS_ANDROID
#include "AndroidPlatformBridge.h"
#endif
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QQuickWindow>
#include <QTimer>
#include <QFontDatabase>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QtMath>
#include <QSurfaceFormat>
#include <QTextCursor>
#include <cstdio>
#include "IntroSceneDiagnostics.h"
#ifndef Q_OS_ANDROID
#include "HostVideoInspection.h"
#include "ExportUiSmoke.h"
#include "BatchExportUiSmoke.h"
#include "CoverExportUiSmoke.h"
#include "CoverBatchExportUiSmoke.h"
#endif

class MobileNoteImages final : public QQuickImageProvider {
public:
    MobileNoteImages() : QQuickImageProvider(Image), icons_(miacode::timeline::loadTimelineNoteAssets(
        miacode::assets::assetPath("skin/skinDX"))) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override {
        const auto type = id.section('?', 0, 0) == "break" ? QStringLiteral("tap_break") : id.section('?', 0, 0);
        const auto image = icons_.noteIcons.value(type).toImage();
        if (size) *size = image.size();
        return requested.isValid() ? image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image;
    }
private:
    miacode::timeline::TimelineNoteAssetSet icons_;
};

int main(int argc, char* argv[])
{
#ifdef Q_OS_ANDROID
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
    QSurfaceFormat::setDefaultFormat(format);
#endif
    QGuiApplication app(argc, argv);
    app.setOrganizationName("MiaCode");
    app.setApplicationName("MiaCodeAndroid");
    QFont uiFont = app.font();
    uiFont.setPixelSize(13);
    app.setFont(uiFont);
    QQuickStyle::setStyle("Basic");
    const QStringList args = app.arguments();
    if (args.contains("--settings-ui-smoke")) app.setApplicationName("MiaCodeMobileSettingsUiSpec");
    if (args.contains("--sfx-playback-smoke")) app.setApplicationName("MiaCodeMobileSfxSpec");
    if (args.contains("--background-ui-smoke")) app.setApplicationName("MiaCodeMobileBackgroundSpec");
    if (args.contains("--workbench-ui-smoke")) app.setApplicationName("MiaCodeMobileWorkbenchUiSpec");
    if (args.contains("--layout-ui-smoke")) app.setApplicationName("MiaCodeMobileLayoutUiSpec");
    if (args.contains("--preferences-dialog-ui-smoke")) app.setApplicationName("MiaCodeMobilePreferencesDialogUiSpec");
    const int preferencesIndex = args.indexOf("--preferences-smoke");
    QString preferencesMode = preferencesIndex >= 0 ? args.value(preferencesIndex + 1) : QString();
    if (preferencesIndex >= 0) app.setApplicationName("MiaCodeMobilePreferencesSpec");
    // Diagnostic comparison for Android's threaded window-obscurity wait.
    // Product launches retain Qt's default render loop.
    if (args.contains("--basic-preview-render-loop")) qputenv("QSG_RENDER_LOOP", "basic");
    if (args.contains("--trace-render-loop")) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "qt.scenegraph.renderloop.debug=true\n"
            "qt.rhi.general.debug=true\n"
            "qt.multimedia.ffmpeg.playbackengine.debug=true\n"
            "qt.multimedia.ffmpeg.streamdecoder.debug=true\n"
            "miacode.android.preview.debug=true\n"
            "miacode.android.window.debug=true\n"
            "miacode.timeline.overlay.debug=true\n"
            "miacode.android.files.debug=true\n"
            "miacode.cover.batch.debug=true\n"
            "miacode.cover.composite.debug=true\n"
            "miacode.cover.composition.debug=true"));
    }
    // Diagnostic comparison for decoder teardown; normal launches retain
    // Qt's hardware decoder selection.
    if (args.contains("--software-preview-decoding"))
        qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ",");
#ifndef Q_OS_ANDROID
    if (args.contains("--export-ui-smoke")) app.setApplicationName("MiaCodeMobileExportUiSpec");
    if (args.contains("--batch-ui-smoke")) app.setApplicationName("MiaCodeMobileBatchUiSpec");
    if (args.contains("--cover-ui-smoke")) app.setApplicationName("MiaCodeMobileCoverUiSpec");
    if (args.contains("--cover-batch-ui-smoke")) app.setApplicationName("MiaCodeMobileCoverBatchUiSpec");
#endif
    std::fprintf(stderr, "Mobile startup: chartExportSmoke=%s\n", args.contains("--export-smoke") ? "true" : "false");
#ifndef Q_OS_ANDROID
    const int inspectionIndex = args.indexOf("--inspect-video");
    if (inspectionIndex >= 0 && inspectionIndex + 2 < args.size())
        return inspectExportVideo(app, args.at(inspectionIndex + 1), args.at(inspectionIndex + 2));
#endif
#ifdef Q_OS_WIN
    if (args.contains("--trace-crash")) {
        extern void enableHostCrashTrace();
        enableHostCrashTrace();
    }
#endif
#ifndef Q_OS_ANDROID
    // The offscreen Windows QPA has no system font discovery. Use a local
    // system font for host layout captures; Android keeps its native fallback.
    if (args.contains("--capture")) {
        const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        const auto families = QFontDatabase::applicationFontFamilies(font);
        if (!families.isEmpty()) {
            uiFont.setFamily(families.first());
            app.setFont(uiFont);
        }
    }
#endif
    const int storageIndex = args.indexOf("--storage-root");
    const QString diagnosticRoot = args.value(storageIndex + 1);
    if (preferencesIndex >= 0) {
        if (storageIndex < 0 || diagnosticRoot.isEmpty()) return 52;
        if (preferencesMode == "auto") {
            QFile modeFile(diagnosticRoot + "/next-mode.txt");
            if (!modeFile.open(QIODevice::ReadOnly)) return 52;
            preferencesMode = QString::fromUtf8(modeFile.readAll()).trimmed();
        }
        miacode::android::preparePreferencesSmoke(preferencesMode);
    }
    const QString sessionStorageRoot = storageIndex >= 0 && storageIndex + 1 < args.size()
        ? args.at(storageIndex + 1) : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    miacode::android::AndroidDocumentSession session(sessionStorageRoot);
    PreferenceDocument::setPreferencesFilePath(sessionStorageRoot + "/preferences.json");
#ifdef Q_OS_ANDROID
    if (storageIndex < 0) {
        QString error;
        const QString previousPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/preferences.json";
        if (!PreferenceDocument::migrateFromFile(previousPath, QStringLiteral("android_config_preferences_v1"), &error)) {
            qCritical().noquote() << error;
            return 63;
        }
    }
#endif
    const int workbenchSmokeIndex = args.indexOf("--workbench-ui-smoke");
    if (workbenchSmokeIndex >= 0 && (storageIndex < 0 || diagnosticRoot.isEmpty()
        || !miacode::android::prepareWorkbenchUiSmoke(args.value(workbenchSmokeIndex + 1)))) return 60;
    const int layoutSmokeIndex = args.indexOf("--layout-ui-smoke");
    if (layoutSmokeIndex >= 0 && (storageIndex < 0 || diagnosticRoot.isEmpty()
        || !miacode::android::prepareLayoutUiSmoke(args.value(layoutSmokeIndex + 1)))) return 61;
    const int preferencesDialogSmokeIndex = args.indexOf("--preferences-dialog-ui-smoke");
    if (preferencesDialogSmokeIndex >= 0 && (storageIndex < 0 || diagnosticRoot.isEmpty()
        || !miacode::android::preparePreferencesDialogUiSmoke(args.value(preferencesDialogSmokeIndex + 1)))) return 62;
    miacode::android::MobilePreferencesStore::preparePlatformDefaultsAndDecoder();
    auto& locale = miacode::LocaleService::instance();
    locale.applyResolvedLanguage();
    if ((args.contains("--export-smoke") || args.contains("--editor-sync-ui-smoke") || args.contains("--settings-ui-smoke") || args.contains("--sfx-playback-smoke"))
        && session.recoveryAvailable()) session.recover();
    if (args.contains("--background-ui-smoke") && session.recoveryAvailable()) session.recover();
    if (args.contains("--workbench-ui-smoke") && session.recoveryAvailable()) session.recover();
    if (args.contains("--layout-ui-smoke") && session.recoveryAvailable()) session.recover();
    if (args.contains("--preferences-dialog-ui-smoke") && session.recoveryAvailable()) session.recover();
#ifndef Q_OS_ANDROID
    const int fixtureIndex = args.indexOf("--fixture");
    if (fixtureIndex >= 0 && fixtureIndex + 1 < args.size() && !session.loadHostFixture(args.at(fixtureIndex + 1))) return 6;
#endif
#ifdef Q_OS_ANDROID
    miacode::android::AndroidPlatformBridge bridge(&session);
#else
    QObject::connect(&session, &miacode::android::AndroidDocumentSession::ioRequested,
        &session, [&session](const QString& kind, const QString&, const QString&) {
            session.completeIo(QJsonObject{{"kind", kind}, {"ok", false},
                {"error", "系统文件选择和媒体探针需在安卓设备运行"}});
        });
#endif
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &session,
        [&session](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive && !session.recoveryAvailable()) session.flushRecovery();
        });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &session, [&session] {
        if (!session.recoveryAvailable()) session.flushRecovery();
    });
    miacode::android::AndroidEditorTools editorTools;
    miacode::ui::EditorController controller;
    miacode::EditorSyncController editorSync;
    miacode::ui::ShortcutModel shortcuts;
    miacode::ui::WorkbenchSettings workspacePreferences;
    const auto applyEditorPreferences = [&] {
        controller.setHalfWidthInputEnabled(workspacePreferences.editorHalfWidthInputEnabled());
        controller.setOverwriteMode(workspacePreferences.editorOverwriteModeEnabled());
        controller.setAutoCompletionEnabled(workspacePreferences.editorAutoCompletionEnabled());
        controller.setImeInputDisabled(workspacePreferences.editorImeInputDisabled());
    };
    QObject::connect(&workspacePreferences, &miacode::ui::WorkbenchSettings::editorSettingsChanged,
        &controller, applyEditorPreferences);
    QObject::connect(&session, &miacode::android::AndroidDocumentSession::changed,
        &workspacePreferences, [&] { workspacePreferences.setEditorFontFamilyOverride(session.editorFont()); });
    workspacePreferences.setEditorFontFamilyOverride(session.editorFont());
    applyEditorPreferences();
    miacode::android::MobilePreview preview(&session);
    preview.setHidePv(workspacePreferences.previewHidePv());
    QObject::connect(&workspacePreferences, &miacode::ui::WorkbenchSettings::previewHidePvChanged,
        &preview, [&] { preview.setHidePv(workspacePreferences.previewHidePv()); });
    miacode::android::MobileVideoExport chartExport(session, preview);
    miacode::android::MobileExportComposition exportComposition(session, preview, chartExport);
    miacode::ui::AppBackgroundModel appBackground(
        qobject_cast<miacode::UiRequestService*>(exportComposition.requests()),
        [] {
            QSettings settings;
            return QJsonDocument::fromJson(settings.value("mobile/uiPreferences").toByteArray()).object();
        },
        [](const QJsonObject& root) {
            QSettings settings;
            settings.setValue("mobile/uiPreferences", QJsonDocument(root).toJson(QJsonDocument::Compact));
            settings.sync();
            return settings.status() == QSettings::NoError;
        });
    if (preferencesIndex >= 0) {
        const int input = args.indexOf("--preferences-fixture");
        return miacode::android::runPreferencesSmoke(preferencesMode, diagnosticRoot, args.value(input + 1),
            session, preview, exportComposition) ? 0 : 53;
    }
    miacode::android::MobileCoverComposition coverComposition(session, preview, exportComposition);
    miacode::android::MobileZipExport zipExport(session,
        *qobject_cast<miacode::UiRequestService*>(exportComposition.requests()),
        *qobject_cast<miacode::JobProgressService*>(exportComposition.progress()));
#ifdef Q_OS_ANDROID
    miacode::android::AndroidFileRequests fileRequests(*qobject_cast<miacode::UiRequestService*>(exportComposition.requests()));
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::uiFileResult,
        &fileRequests, &miacode::android::AndroidFileRequests::deliver);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::exportPublicationUpdate,
        &chartExport, &miacode::android::MobileVideoExport::publicationUpdate);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::exportPublicationUpdate,
        &coverComposition, &miacode::android::MobileCoverComposition::publicationUpdate);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::exportPublicationUpdate,
        &zipExport, &miacode::android::MobileZipExport::publicationUpdate);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::chartExportCancelled,
        &chartExport, &miacode::android::MobileVideoExport::cancel);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::chartExportCancelled,
        &coverComposition, &miacode::android::MobileCoverComposition::cancelBatch);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::chartExportCancelled,
        &zipExport, &miacode::android::MobileZipExport::cancel);
#endif
    const auto validationLocale = [](const QString& language) {
        return language == "zh" ? SimaiNativeValidationLocale::Chinese
            : language == "ja" ? SimaiNativeValidationLocale::Japanese : SimaiNativeValidationLocale::English;
    };
    miacode::AnalysisService analysis(session.workspace(), validationLocale(locale.activeLanguageToken()));
    QObject::connect(&locale, &miacode::LocaleService::languageChanged, &analysis,
        [&analysis, validationLocale](const QString& language) { analysis.setLocale(validationLocale(language)); });
    miacode::ui::AnalysisModel analysisModel(session.workspace(), analysis);
    miacode::android::MobileTimeline timeline(session, preview, editorSync, analysis);
    miacode::android::MobileLatency latencyEngine(session, preview);
    miacode::LatencyEngine* latencyEngineSlot = &latencyEngine;
    miacode::ui::LatencyModel latencyModel(latencyEngineSlot);
    QObject::connect(&session, &miacode::android::AndroidDocumentSession::documentStateChanged,
        &latencyModel, [&] { if (latencyEngine.isOnPage()) latencyModel.refreshFromDocument(); });
    miacode::android::MobilePreferencesStore preferencesStore(workspacePreferences, preview, timeline);
    miacode::PreferencesStore* preferencesStoreSlot = &preferencesStore;
    miacode::ui::PreferencesModel preferencesModel(preferencesStoreSlot, workspacePreferences);
    QObject::connect(&preferencesStore, &miacode::android::MobilePreferencesStore::refreshRateChanged,
        &preferencesModel, &miacode::ui::PreferencesModel::performanceChanged);
    miacode::update::NetworkUpdateFetcher updateFetcher;
    miacode::update::PreferenceUpdateStateStore updateState;
    miacode::update::UpdateService updates(updateFetcher, updateState,
        {QStringLiteral(MIACODE_VERSION_STRING), MIACODE_VERSION_MAJOR,
            QStringLiteral("android-arm64"), locale.activeLanguageToken()});
    QObject::connect(&locale, &miacode::LocaleService::languageChanged,
        &updates, &miacode::update::UpdateService::setLanguageToken);
    QObject::connect(&preview, &miacode::android::MobilePreview::muriParametersChanged, &analysis, [&] {
        analysis.setMuriParameters(preview.muriRenderOptions(), preview.muriTapOnSlideThresholdMs() / 1000.0);
    });
    const int secondIndex = args.indexOf("--preview-second");
    if (secondIndex >= 0 && secondIndex + 1 < args.size()) preview.setPositionSeconds(args.at(secondIndex + 1).toDouble());
    if (args.contains("--muri")) preview.setMuriCheckEnabled(true);
    qmlRegisterType<PreviewQuickSceneRoot>("MiaCode.Preview", 1, 0, "PreviewQuickSceneRoot");
    qmlRegisterType<PreviewQuickHudLayer>("MiaCode.Preview", 1, 0, "PreviewQuickHudLayer");
    qmlRegisterType<TimelineQuickItem>("MiaCode.Timeline", 1, 0, "TimelineQuickItem");
    QQmlApplicationEngine engine;
    locale.setQmlEngine(&engine);
    QObject::connect(&engine, &QObject::destroyed, &locale, [&locale] { locale.setQmlEngine(nullptr); });
    engine.addImageProvider("noteicon", new MobileNoteImages);
    miacode::intro::registerIntroAssetImages(&engine);
    miacode::cover_export::registerCoverChartImageProvider(&engine, coverComposition.coverSession().coverLayout());
    engine.rootContext()->setContextProperty("androidSession", &session);
    engine.rootContext()->setContextProperty("editorTools", &editorTools);
    engine.rootContext()->setContextProperty("v2EditorController", &controller);
    engine.rootContext()->setContextProperty("mobileEditorSync", &editorSync);
    engine.rootContext()->setContextProperty("mobilePreview", &preview);
    engine.rootContext()->setContextProperty("mobileTimeline", &timeline);
    engine.rootContext()->setContextProperty("mobileLatency", &latencyModel);
    engine.rootContext()->setContextProperty("mobileAnalysis", &analysisModel);
    engine.rootContext()->setContextProperty("mobileShortcuts", &shortcuts);
    engine.rootContext()->setContextProperty("mobileChartExport", &chartExport);
    engine.rootContext()->setContextProperty("mobileExport", &exportComposition);
    engine.rootContext()->setContextProperty("mobileCover", &coverComposition);
    engine.rootContext()->setContextProperty("mobileZipExport", &zipExport);
    engine.rootContext()->setContextProperty("mobileAppBackground", &appBackground);
    engine.rootContext()->setContextProperty("mobilePreferences", &workspacePreferences);
    engine.rootContext()->setContextProperty("mobilePreferencesStore", &preferencesStore);
    engine.rootContext()->setContextProperty("mobilePreferencesModel", &preferencesModel);
    engine.rootContext()->setContextProperty("mobileUpdates", &updates);
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &preview,
        [&preview](Qt::ApplicationState state) { if (state != Qt::ApplicationActive) preview.setPlaying(false); });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("MiaCode.UI", "AndroidMain");
    if (args.contains("--trace-intro-scene") && !engine.rootObjects().isEmpty()) {
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first()))
            miacode::android::enableIntroSceneDiagnostics(*window, preview, sessionStorageRoot);
    }
#ifdef Q_OS_ANDROID
    if (!engine.rootObjects().isEmpty()) {
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first()))
            new miacode::android::MobileWindowLifecycle(*window, [&coverComposition] {
                return coverComposition.batchController().running();
            });
    }
#endif
    if (args.size() == 1) updates.scheduleStartupCheck();
    if (preferencesDialogSmokeIndex >= 0)
        miacode::android::startPreferencesDialogUiSmoke(app, engine, preferencesModel, workspacePreferences,
            controller, preferencesStore, diagnosticRoot, args.value(preferencesDialogSmokeIndex + 1));
    if (workbenchSmokeIndex >= 0)
        miacode::android::startWorkbenchUiSmoke(app, engine, workspacePreferences, controller, preview, timeline,
            diagnosticRoot, args.value(workbenchSmokeIndex + 1));
    if (layoutSmokeIndex >= 0)
        miacode::android::startLayoutUiSmoke(app, engine, workspacePreferences,
            diagnosticRoot, args.value(layoutSmokeIndex + 1));
    if (args.contains("--background-ui-smoke")) {
        const int modeIndex = args.indexOf("--background-ui-smoke");
        if (storageIndex < 0 || diagnosticRoot.isEmpty()) return 59;
        miacode::android::startBackgroundUiSmoke(app, engine, session, appBackground,
            diagnosticRoot, args.value(modeIndex + 1));
    }
#ifndef Q_OS_ANDROID
    if (args.contains("--export-ui-smoke"))
        miacode::android::startExportUiSmoke(app, engine, session, preview, chartExport, exportComposition);
    if (args.contains("--batch-ui-smoke"))
        miacode::android::startBatchExportUiSmoke(app, engine, session, chartExport, exportComposition);
    if (args.contains("--cover-ui-smoke"))
        miacode::android::startCoverExportUiSmoke(app, engine, session, coverComposition);
    if (args.contains("--cover-batch-ui-smoke"))
        miacode::android::startCoverBatchExportUiSmoke(app, engine, session, coverComposition);
    if (args.contains("--export-smoke") && !engine.rootObjects().isEmpty()) {
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) window->setVisible(false);
    }
#endif
    if (args.contains("--export-smoke")) {
        QObject::connect(&chartExport, &miacode::android::MobileVideoExport::finished, &app,
            [&app](bool success, const QString& output, const QString& error) {
                std::fprintf(stderr, "Chart export smoke: %s; output=%s; error=%s\n",
                    success ? "passed" : "failed", qPrintable(output), qPrintable(error));
                app.exit(success ? 0 : 30);
            });
        QTimer::singleShot(1000, &app, [&chartExport, &app, args] {
            auto task = chartExport.buildTask();
            task.outputWidth = 720; task.outputHeight = 720; task.fps = 30;
            task.exportStartSeconds = 10; task.contentDurationSeconds = 5; task.fullRangeExport = false;
            const int settingsIndex = args.indexOf("--export-settings");
            if (settingsIndex >= 0) {
                QFile file(settingsIndex + 1 < args.size() ? args.at(settingsIndex + 1) : QString());
                if (!file.open(QIODevice::ReadOnly)) { qCritical() << "Cannot read export smoke settings"; app.exit(32); return; }
                const auto settings = QJsonDocument::fromJson(file.readAll());
                if (!settings.isObject()) { qCritical() << "Invalid export smoke settings"; app.exit(32); return; }
                miacode::video_export::applyVideoExportPreferences(settings.object(), &task);
            }
            const int durationIndex = args.indexOf("--export-smoke-seconds");
            if (durationIndex >= 0) {
                bool valid = false;
                const double duration = durationIndex + 1 < args.size() ? args.at(durationIndex + 1).toDouble(&valid) : 0;
                if (!valid || !qIsFinite(duration) || duration <= 0) { qCritical() << "Invalid export smoke duration"; app.exit(32); return; }
                task.contentDurationSeconds = duration;
            }
            const int outputIndex = args.indexOf("--export-output");
            if (outputIndex >= 0 && outputIndex + 1 < args.size()) task.outputPath = args.at(outputIndex + 1);
            if (args.contains("--export-intro")) {
                task.exportStartSeconds = 0; task.contentDurationSeconds = 5;
                task.fullRangeExport = true; task.intro.enabled = true;
            }
            if (args.contains("--export-static")) task.backgroundMediaPath = chartExport.buildTask().intro.jacketPath;
            QString error;
            if (!chartExport.start(task, &error)) { qCritical() << "Chart export launch failed:" << error; app.exit(31); }
        });
    }
    if (args.contains("--layout-smoke") && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        window->resize(807, 345);
        QTimer::singleShot(700, &app, [window, &app, &preview] {
            auto* pane = window->findChild<QQuickItem*>("v2PreviewPane");
            auto* stats = window->findChild<QQuickItem*>("previewNoteStatistics");
            if (!pane || !stats) { app.exit(20); return; }
            const double baseline = window->property("workbenchScale").toDouble();
            auto* timer = new QTimer(window);
            auto step = std::make_shared<int>(0);
            auto seenRows = std::make_shared<int>(0);
            preview.setPlaying(true);
            QObject::connect(timer, &QTimer::timeout, window, [pane, stats, window, timer, step, seenRows, baseline, &app] {
                const double scale = window->property("workbenchScale").toDouble();
                const int rows = stats->property("rows").toInt();
                const int capacity = stats->property("columns").toInt() * rows;
                if (rows == 1 || rows == 2) *seenRows |= 1 << (rows - 1);
                if (qAbs(scale - baseline) > 0.000001 || capacity < 6) {
                    std::fprintf(stderr, "Layout smoke: preview resize changed workbench scale or invalidated the grid\n");
                    timer->stop(); app.exit(21); return;
                }
                if (*step == 18) {
                    if (*seenRows != 3) {
                        std::fprintf(stderr, "Layout smoke: did not exercise both statistics layouts\n");
                        timer->stop(); app.exit(23); return;
                    }
                    std::printf("v2 Android layout: 18 resizes across statistics breakpoints: passed\n");
                    timer->stop(); app.exit(0); return;
                }
                const double widths[] = {520, 536, 420, 610, 527, 529};
                QQmlProperty preferredWidth(pane, "SplitView.preferredWidth", qmlContext(pane));
                if (!preferredWidth.write(widths[(*step)++ % 6])) {
                    timer->stop(); app.exit(22);
                }
            });
            timer->start(100);
        });
    }
    const int captureIndex = args.indexOf("--capture");
    if (captureIndex >= 0 && captureIndex + 1 < args.size() && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        const int sizeIndex = args.indexOf("--size");
        if (window && sizeIndex >= 0 && sizeIndex + 1 < args.size()) {
            const auto size = args.at(sizeIndex + 1).split('x');
            if (size.size() == 2) window->resize(size.at(0).toInt(), size.at(1).toInt());
        }
        QTimer::singleShot(1000, &app, [window, &app, args, captureIndex] {
            const bool ok = window && window->grabWindow().save(args.at(captureIndex + 1));
            app.exit(ok ? 0 : 2);
        });
    }
    if (args.contains("--editor-sync-ui-smoke"))
        miacode::android::startEditorSyncUiSmoke(app, engine, session, preview, timeline, editorSync);
    if (args.contains("--settings-ui-smoke"))
        miacode::android::startSettingsUiSmoke(app, engine, session, preview, exportComposition);
    if (args.contains("--sfx-playback-smoke"))
        miacode::android::startSfxPlaybackSmoke(app, engine, session, preview, exportComposition);
    if (args.contains("--ui-smoke") && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(500, &app, [&engine, &session, &app, &controller, &editorSync] {
            auto* view = engine.rootObjects().first()->findChild<QQuickItem*>("v2SourceEditor");
            if (!view || view->property("syncController").value<QObject*>() != &editorSync) {
                std::fprintf(stderr, "UI smoke: source editor is not connected to the shared editor sync controller\n");
                app.exit(27);
                return;
            }
            auto* editor = engine.rootObjects().first()->findChild<QQuickItem*>("sourceArea");
            auto* quick = editor ? editor->property("textDocument").value<QQuickTextDocument*>() : nullptr;
            auto* document = quick ? quick->textDocument() : nullptr;
            if (!document || controller.canUndo()) {
                std::fprintf(stderr, "UI smoke: initial document has invalid undo state\n");
                app.exit(3);
                return;
            }
            editor->forceActiveFocus();
            QTextCursor cursor(document);
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(",7");
            if (!session.dirty() || !session.chartText().endsWith(",7")) {
                std::fprintf(stderr, "UI smoke: editor did not publish the edit\n");
                app.exit(4);
                return;
            }
            const int originalDifficulty = session.activeDifficulty();
            session.addDifficulty(6);
            session.selectDifficulty(6);
            QTimer::singleShot(100, &app, [&engine, &session, &app, &controller, originalDifficulty] {
                if (controller.canUndo() || !session.chartText().isEmpty()) { app.exit(5); return; }
                session.selectDifficulty(originalDifficulty);
                QTimer::singleShot(100, &app, [&engine, &session, &app, &controller] {
                    auto* view = engine.rootObjects().first()->findChild<QQuickItem*>("v2SourceEditor");
                    const bool restored = controller.canUndo() && session.chartText().endsWith(",7");
                    if (restored && view) QMetaObject::invokeMethod(view, "undo");
                    const bool ok = restored && !session.chartText().endsWith(",7");
                    std::printf("v2 Android editor: typing, difficulty isolation, retained undo: %s\n", ok ? "passed" : "failed");
                    app.exit(ok ? 0 : 7);
                });
            });
        });
    }
    return app.exec();
}
