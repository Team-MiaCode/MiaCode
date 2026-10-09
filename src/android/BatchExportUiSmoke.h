#pragma once
#include "MobileExportComposition.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <cstdio>

namespace miacode::android {
inline void startBatchExportUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, MobileVideoExport& exporter, MobileExportComposition& composition) {
    if (engine.rootObjects().isEmpty()) { app.exit(60); return; }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* session = qobject_cast<ui::ExportSession*>(composition.session());
    auto* requests = qobject_cast<UiRequestService*>(composition.requests());
    const QString proof = QFileInfo(document.currentFilePath()).absolutePath() + "/batch-ui-proof";
    QDir().mkpath(proof);
    QTimer::singleShot(500, &app, [window] { window->setProperty("activePage", "export"); });
    QTimer::singleShot(1500, &app, [&, window, session, requests, proof] {
        if (!session->pageSessionActive()) { app.exit(61); return; }
        for (auto* child : window->findChildren<QObject*>())
            if (child->metaObject()->indexOfProperty("externalFileDialogs") >= 0) child->setProperty("externalFileDialogs", true);
        const auto makeChart = [&](const QString& name, const QString& first) {
            const QString path = proof + "/inputs/" + name; QDir().mkpath(path);
            QFile chart(path + "/maidata.txt");
            if (!chart.open(QIODevice::WriteOnly)) return QString();
            chart.write(("&title=Batch same title\n&first=" + first + "\n&lv_5=13\n&inote_5=(120){4}1,2,E\n").toUtf8()); chart.close();
            QByteArray bytes; QDataStream stream(&bytes, QIODevice::WriteOnly); stream.setByteOrder(QDataStream::LittleEndian);
            stream.writeRawData("RIFF", 4); stream << quint32(16036); stream.writeRawData("WAVEfmt ", 8);
            stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
            stream.writeRawData("data", 4); stream << quint32(16000); bytes.append(QByteArray(16000, '\0'));
            QFile audio(path + "/track.wav"); if (!audio.open(QIODevice::WriteOnly)) return QString(); audio.write(bytes);
            return path;
        };
        QStringList inputs{makeChart("first", "0.25"), makeChart("invalid", "nan"), makeChart("second", "0.25")};
        if (inputs.contains(QString())) { app.exit(62); return; }
        int inputIndex = 0;
        const auto picker = QObject::connect(requests, &UiRequestService::fileRequested, &app,
            [&](const QString& id, const QVariantMap& request) {
                requests->submitFileResult(id, QUrl::fromLocalFile(request.value("saveMode").toBool()
                    ? proof + "/completed" : inputs.at(inputIndex++)));
            });
        QVariantMap notice; QString noticeId; int notices = 0;
        const auto noticeConnection = QObject::connect(requests, &UiRequestService::noticeRequested, &app,
            [&](const QString& id, const QVariantMap& value) { notice = value; noticeId = id; ++notices; });
        session->setActiveTab("batch"); session->clearChartDirectories();
        for (int id = 1; id <= 7; ++id) session->setBatchDifficultyChecked(id, id == 5);
        for (int i = 0; i < inputs.size(); ++i) session->addChartDirectories();
        session->browseBatchOutputDirectory();
        QObject::disconnect(picker);
        session->setOutputPath(proof + "/format.wav"); session->setIntroEnabled(false);
        session->setExportRangeSeconds(10, 15);
        // An invalid, unsaved LIVE chart must not poison valid batch inputs.
        document.setChartText("(120){4}8,7,6,E"); document.setMetadataFirst("nan");
        const auto before = document.workspace().snapshot();
        window->grabWindow().save(proof + "/batch-page.png");
        session->startExport();
        const auto completed = QDir(proof + "/completed").entryList({"*.wav"}, QDir::Files);
        const auto after = document.workspace().snapshot();
        const bool untouched = before.sourceText == after.sourceText && before.revision == after.revision
            && before.filePath == after.filePath && before.activeDifficultyId == after.activeDifficultyId && after.dirty;
        const bool partial = completed.size() == 2 && notices == 1
            && notice.value("text").toString() == qtTrId("dialog.batch_export.message.partial_failed").arg(2).arg(1)
            && !session->exportRunning() && !exporter.running();
        requests->submitNoticeResult(noticeId, false);
        // Notification cancellation reaches the exporter during preparation,
        // before there is an active per-chart codec/audio task.
        auto preparationSettings = composition.buildSeedTask(5);
        preparationSettings.outputPath = proof + "/format.wav";
        ExportEngine::BatchResult preparationResult;
        ExportEngine::BatchCallbacks preparationCallbacks;
        bool canceledPreparation = false;
        preparationCallbacks.progressChanged = [&](int percent, const QString&) {
            if (!canceledPreparation && percent == 0) {
                canceledPreparation = true; exporter.cancel();
            }
        };
        QString preparationError;
        const bool preparationLaunched = composition.launchBatchExport(preparationSettings, inputs, {5},
            proof + "/preparation-canceled", &preparationResult, preparationCallbacks, &preparationError);
        const bool preparationCanceled = preparationLaunched && canceledPreparation && preparationResult.canceled
            && preparationResult.successCount == 0 && preparationError.isEmpty() && !exporter.running()
            && QDir(proof + "/preparation-canceled").entryList({"*.wav"}, QDir::Files).isEmpty();
        bool cancelOnStart = true;
        const auto cancelConnection = QObject::connect(&exporter, &MobileVideoExport::changed, &app, [&] {
            if (cancelOnStart && exporter.running()) { cancelOnStart = false; session->cancelExport(); }
        });
        session->setBatchOutputDirectory(proof + "/canceled"); session->startExport();
        QObject::disconnect(cancelConnection);
        const bool cancellation = !cancelOnStart && !exporter.running() && !session->exportRunning()
            && notice.value("text").toString() == qtTrId("dialog.batch_export.message.canceled")
            && QDir(proof + "/canceled").entryList({"*.wav"}, QDir::Files).size() < 2;
        requests->submitNoticeResult(noticeId, false);
        bool nativeCancelOnStart = true;
        const auto nativeCancelConnection = QObject::connect(&exporter, &MobileVideoExport::changed, &app, [&] {
            if (nativeCancelOnStart && exporter.running()) { nativeCancelOnStart = false; exporter.cancel(); }
        });
        session->setBatchOutputDirectory(proof + "/notification-canceled"); session->startExport();
        QObject::disconnect(nativeCancelConnection);
        const bool nativeCancellation = !nativeCancelOnStart && !exporter.running() && !session->exportRunning()
            && notice.value("text").toString() == qtTrId("dialog.batch_export.message.canceled")
            && QDir(proof + "/notification-canceled").entryList({"*.wav"}, QDir::Files).isEmpty();
        requests->submitNoticeResult(noticeId, false);
        session->setBatchOutputDirectory(proof + "/resumed"); session->startExport();
        const bool resumed = QDir(proof + "/resumed").entryList({"*.wav"}, QDir::Files).size() == 2
            && !exporter.running() && !session->exportRunning();
        QObject::disconnect(noticeConnection);
        const auto finalState = document.workspace().snapshot();
        const bool stillUntouched = untouched && before.sourceText == finalState.sourceText
            && before.revision == finalState.revision && finalState.dirty;
        QJsonArray files; for (const auto& name : completed) files.append(proof + "/completed/" + name);
        const bool passed = partial && preparationCanceled && cancellation && nativeCancellation && resumed && stillUntouched;
        QSaveFile report(proof + "/verification.json");
        if (report.open(QIODevice::WriteOnly)) {
            report.write(QJsonDocument(QJsonObject{{"success", passed}, {"partialFailureSummary", partial},
                {"cancelWhileRunning", cancellation}, {"resumedAfterCancel", resumed}, {"liveWorkspaceUnchanged", stillUntouched},
                {"nativeCancelDuringPreparation", preparationCanceled}, {"nativeCancelWhileRunning", nativeCancellation},
                {"outputFiles", files}, {"source", "actual v2 ExportSession batch page"}}).toJson()); report.commit();
        }
        std::fprintf(stderr, "Batch UI smoke: %s; partial=%d canceled=%d resumed=%d unchanged=%d; proof=%s\n",
            passed ? "passed" : "failed", partial, cancellation, resumed, stillUntouched, qPrintable(proof));
        app.exit(passed ? 0 : 63);
    });
    QTimer::singleShot(90000, &app, [&app, &exporter] { exporter.cancel(); app.exit(64); });
}
}
