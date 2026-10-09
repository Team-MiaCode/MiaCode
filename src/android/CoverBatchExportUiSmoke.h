#pragma once
#include "CoverExportUiSmoke.h"
#include "tools/cover_export/CoverCompositionState.h"
#include <QCryptographicHash>

namespace miacode::android {
// Host invocation uses QSG_RHI_BACKEND=opengl and QSG_RENDER_LOOP=basic.
// Android's actual render loop and SAF publication are verified separately.
inline void startCoverBatchExportUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, MobileCoverComposition& composition)
{
    auto* window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) { app.exit(100); return; }
    const QString proof = QFileInfo(document.currentFilePath()).absolutePath() + "/cover-batch-ui-proof";
    QDir().mkpath(proof);
    auto& session = composition.coverSession();
    auto& batch = composition.batchController();
    const auto secondChart = QStringLiteral("(120){4}1,2,3,4,E");
    if (!document.workspace().addDifficulty(3) || !document.workspace().replaceDifficultyChart(3, secondChart)) {
        app.exit(101); return;
    }
    document.workspace().selectDifficulty(5);
    auto before = std::make_shared<ChartWorkspaceSnapshot>();
    auto layoutBefore = std::make_shared<QJsonObject>();
    auto preferencesBefore = std::make_shared<QJsonObject>();
    auto outputs = std::make_shared<QJsonArray>();
    auto phase = std::make_shared<int>(0);
    QObject::connect(&batch, &cover_export::CoverBatchExport::changed, &app, [&batch] {
        const auto text = QJsonDocument::fromVariant(batch.results()).toJson(QJsonDocument::Compact);
        std::fprintf(stderr, "Batch changed: running=%d completed=%d total=%d %s\n",
            batch.running(), batch.completed(), batch.total(), text.constData());
    });
    QObject::connect(&batch, &cover_export::CoverBatchExport::finished, &app,
        [&, window, proof, before, layoutBefore, preferencesBefore, outputs, phase] {
        const auto rows = batch.results();
        int success = 0, failed = 0, canceled = 0;
        QJsonArray jobs;
        for (const auto& row : rows) {
            const auto value = row.toMap();
            const auto status = value.value("status").toString();
            const QString path = value.value("path").toString();
            if (status == "success") {
                QImage image(path);
                if (image.isNull() || image.size() != QSize(720, 720)) { app.exit(102); return; }
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) { app.exit(103); return; }
                const auto sha = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
                jobs.append(QJsonObject{{"path", path}, {"sha256", QString::fromLatin1(sha)}, {"status", status}});
                ++success;
            } else if (status == "failed") { ++failed; jobs.append(QJsonObject::fromVariantMap(value)); }
            else if (status == "canceled") ++canceled;
        }
        const auto after = document.workspace().snapshot();
        const bool untouched = before->sourceText == after.sourceText && before->revision == after.revision
            && before->activeDifficultyId == after.activeDifficultyId && before->dirty == after.dirty
            && *layoutBefore == session.coverLayout()->toJson()
            && *preferencesBefore == cover_export::CoverCompositionState::loadPreferences();
        const bool passed = untouched && rows.size() == 4
            && (*phase == 1 ? canceled == 4 && success == 0 : *phase == 3 ? success == 2 && failed == 2 : success == 4);
        outputs->append(QJsonObject{{"phase", *phase}, {"passed", passed}, {"unchanged", untouched},
            {"success", success}, {"failed", failed}, {"canceled", canceled}, {"jobs", jobs}});
        window->grabWindow().save(proof + QStringLiteral("/results-%1.png").arg(*phase));
        if (!passed) { app.exit(104); return; }
        if (*phase == 0) {
            *phase = 1;
            QTimer::singleShot(200, &app, [window, &app] {
                if (!clickCoverUi(window, "choiceDialogButton_start") || !clickCoverUi(window, "choiceDialogButton_close")) app.exit(105);
            });
        } else if (*phase == 1) {
            *phase = 2;
            QTimer::singleShot(200, &app, [window, &app] {
                if (!clickCoverUi(window, "choiceDialogButton_start")) app.exit(106);
            });
        } else if (*phase == 2) {
            *phase = 3;
            QTimer::singleShot(200, &app, [window, &app] {
                if (!clickCoverUi(window, "coverBatchPreset_builtin_pure_chart_frame")
                    || !clickCoverUi(window, "coverBatchPreset_user_Missing image")
                    || !clickCoverUi(window, "choiceDialogButton_start")) app.exit(107);
            });
        } else {
            QSaveFile file(proof + "/verification.json");
            if (!file.open(QIODevice::WriteOnly)) { app.exit(108); return; }
            file.write(QJsonDocument(QJsonObject{{"passed", true}, {"actualUi", true}, {"phases", *outputs},
                {"P5Accepted", false}}).toJson());
            if (!file.commit()) { app.exit(109); return; }
            qInfo() << "Batch cover actual UI passed:" << proof;
            app.exit(0);
        }
    });
    QTimer::singleShot(800, &app, [window] { window->setProperty("coverOpen", true); });
    QTimer::singleShot(1800, &app, [&, window, proof, before, layoutBefore, preferencesBefore] {
        if (!session.pageSessionActive()) { app.exit(110); return; }
        session.setResolutionIndex(0);
        session.setOutputDirectory(proof + "/outputs");
        session.applyBuiltinPreset(QStringLiteral("card"));
        auto missing = session.batchComposition(QStringLiteral("current"), {});
        cover_export::CoverLayoutModel missingLayout;
        missingLayout.fromJson(missing.value("layout").toObject());
        missingLayout.addImageLayer(proof + "/missing.png");
        missing.insert("layout", missingLayout.toJson());
        missing.remove("size"); missing.remove("output");
        cover_export::CoverCompositionState::saveUserPreset("Missing image", missing);
        // Refresh preset options through the same session owner as the product.
        session.enter(5);
        *before = document.workspace().snapshot(); *layoutBefore = session.coverLayout()->toJson();
        *preferencesBefore = cover_export::CoverCompositionState::loadPreferences();
        // Invalid selection must fail before an execution hook or output is created.
        QVariantList invalidDifficulties;
        invalidDifficulties.append(5);
        QVariantMap invalidPreset;
        invalidPreset.insert(QStringLiteral("kind"), QStringLiteral("user"));
        invalidPreset.insert(QStringLiteral("name"), QStringLiteral("Does not exist"));
        QVariantList invalidPresets;
        invalidPresets.append(invalidPreset);
        if (batch.start(invalidDifficulties, invalidPresets)) { app.exit(111); return; }
        if (!clickCoverUi(window, "coverBatchExportButton")) { app.exit(112); return; }
        QTimer::singleShot(400, &app, [window, &app, proof] {
            window->grabWindow().save(proof + "/dialog-open.png");
            for (const auto& name : {QStringLiteral("coverBatchDifficulty_3"), QStringLiteral("coverBatchPreset_builtin_pure_chart_frame")}) {
                auto* item = window->findChild<QQuickItem*>(name);
                std::fprintf(stderr, "Batch control %s: exists=%d visible=%d enabled=%d\n", qPrintable(name), item != nullptr, item && item->isVisible(), item && item->isEnabled());
            }
            if (!clickCoverUi(window, "coverBatchDifficulty_3")
                || !clickCoverUi(window, "coverBatchPreset_builtin_pure_chart_frame")) { app.exit(113); return; }
            window->grabWindow().save(proof + "/selection.png");
            if (!clickCoverUi(window, "choiceDialogButton_start")) app.exit(114);
        });
    });
    QTimer::singleShot(90000, &app, [&app, window, proof] {
        window->grabWindow().save(proof + "/timeout.png");
        std::fprintf(stderr, "Batch cover actual UI timeout\n"); app.exit(115);
    });
}
}
