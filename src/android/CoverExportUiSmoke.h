#pragma once
#include "MobileCoverComposition.h"
#include "AndroidDocumentSession.h"
#include "tools/cover_export/CoverLayoutModel.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <algorithm>

namespace miacode::android {
inline QQuickItem* findCoverUiItem(QQuickItem* parent, const QString& name)
{
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems()) {
        if (auto* found = findCoverUiItem(child, name)) return found;
    }
    return nullptr;
}
inline bool clickCoverUi(QQuickWindow* window, const QString& name)
{
    auto* item = window->findChild<QQuickItem*>(name);
    if (!item) item = findCoverUiItem(window->contentItem(), name);
    if (!item || !item->isVisible() || !item->isEnabled()) return false;
    const QPointF point = item->mapToScene(QPointF(item->width()/2, item->height()/2));
    QMouseEvent press(QEvent::MouseButtonPress, point, window->mapToGlobal(point),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, window->mapToGlobal(point),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    press.setTimestamp(static_cast<ulong>(QDateTime::currentMSecsSinceEpoch()));
    release.setTimestamp(press.timestamp() + 1);
    QCoreApplication::sendEvent(window, &press);
    QCoreApplication::sendEvent(window, &release);
    return true;
}
inline void startCoverExportUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, MobileCoverComposition& composition)
{
    auto* window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) { app.exit(70); return; }
    auto& session = composition.coverSession();
    auto* requests = qobject_cast<UiRequestService*>(session.uiRequests());
    QObject::connect(requests, &UiRequestService::noticeRequested, &app,
        [window](const QString&, const QVariantMap&) {
            if (auto* dialog = window->findChild<QObject*>("uiRequestNoticeDialog"))
                QMetaObject::invokeMethod(dialog, "resolve", Q_ARG(QVariant, QVariant(QStringLiteral("reject"))));
        }, Qt::QueuedConnection);
    const QString directory = QFileInfo(document.currentFilePath()).absolutePath() + "/cover-ui-proof";
    QDir().mkpath(directory);
    auto outputs = std::make_shared<QJsonArray>();
    const auto before = document.workspace().snapshot();
    auto stage = std::make_shared<int>(0);
    QObject::connect(&session, &ui::CoverExportSession::exportFinished, &app,
        [window, &app, &session, &document, outputs, stage, before, directory](bool ok, const QString& path, const QString& error) {
        const QImage image(path);
        const bool transparent = *stage >= 1;
        const bool imagePassed = ok && !image.isNull() && image.size() == QSize(session.outputWidth(), session.outputHeight())
            && (!transparent || (image.hasAlphaChannel() && image.pixelColor(0, 0).alpha() == 0));
        outputs->append(QJsonObject{{"success", imagePassed}, {"path", path}, {"error", error},
            {"width", image.width()}, {"height", image.height()}, {"transparent", transparent}});
        if (!imagePassed) { app.exit(71); return; }
        if ((*stage)++ == 0) {
            QTimer::singleShot(800, &app, [window, &session, directory] {
                session.setBackgroundMode(static_cast<int>(cover_export::CoverBackgroundMode::Transparent));
                window->grabWindow().save(directory + "/transparent-ui.png");
                qInfo() << "Transparent cover UI click:" << clickCoverUi(window, "coverExportButton");
            });
            return;
        }
        if (*stage == 2) {
            QTimer::singleShot(800, &app, [window, &session, &app, directory] {
                session.applyBuiltinPreset(QStringLiteral("dual_chart_frames"));
                const auto frames = session.coverLayout()->visibleChartFrameLayers();
                if (frames.size() != 2) { app.exit(76); return; }
                session.selectLayerKey(frames[0]->key());
                session.setActiveLayerFrameSeconds(7.14);
                session.selectLayerKey(frames[1]->key());
                session.setActiveLayerFrameSeconds(11.68);
                if (!clickCoverUi(window, "coverCloseButton")) { app.exit(77); return; }
                QTimer::singleShot(500, &app, [window, &app, &session, directory] {
                    window->setProperty("coverOpen", true);
                    QTimer::singleShot(800, &app, [window, &app, &session, directory] {
                        const auto reopened = session.coverLayout()->visibleChartFrameLayers();
                        const auto inactive = std::find_if(reopened.begin(), reopened.end(), [&session](auto* layer) {
                            return layer->key() != session.activeLayerKey();
                        });
                        if (reopened.size() != 2 || inactive == reopened.end() || (*inactive)->frameImage().isNull()) {
                            app.exit(78); return;
                        }
                        (*inactive)->frameImage().save(directory + "/reopened-inactive-frame.png");
                        window->grabWindow().save(directory + "/dual-reopened-ui.png");
                        if (!clickCoverUi(window, "coverExportButton")) app.exit(79);
                    });
                });
            });
            return;
        }
        const auto after = document.workspace().snapshot();
        const bool preserved = before.sourceText == after.sourceText && before.revision == after.revision
            && before.activeDifficultyId == after.activeDifficultyId && before.dirty == after.dirty;
        if (!preserved) { app.exit(72); return; }
        const auto layoutBefore = session.coverLayout()->toJson();
        const QString outputBefore = session.outputDirectory();
        const auto inputsCleared = [&session] {
            const auto frames = session.coverLayout()->chartFrameLayers();
            return !session.chartFrameAvailable() && session.chartFrameDuration() == 0
                && !session.liveChartSceneBound() && !session.chartFramePlaying()
                && session.selectedDifficultyId() == 0 && session.difficulties().isEmpty()
                && session.jacketImage().isEmpty()
                && session.trackOverrides().value(QStringLiteral("title")).toString().isEmpty()
                && std::all_of(frames.cbegin(), frames.cend(), [](auto* layer) { return layer->frameImage().isNull(); });
        };
        if (!clickCoverUi(window, "coverCloseButton")) { app.exit(90); return; }
        const auto ids = document.workspace().document().difficultyIds();
        for (int id : ids) {
            if (!document.workspace().removeDifficulty(id)) { app.exit(91); return; }
        }
        window->setProperty("coverOpen", true);
        if (!inputsCleared()) { app.exit(92); return; }
        if (!document.workspace().closeDocument().accepted || !inputsCleared()) { app.exit(80); return; }
        QTimer::singleShot(500, &app, [window, &app, &session, &document, outputs, before, directory,
                                     layoutBefore, outputBefore, inputsCleared] {
            window->setProperty("coverOpen", true);
            QTimer::singleShot(500, &app, [window, &app, &session, &document, outputs, before, directory,
                                         layoutBefore, outputBefore, inputsCleared] {
                if (!session.pageSessionActive() || !inputsCleared()
                    || session.coverLayout()->toJson() != layoutBefore || session.outputDirectory() != outputBefore) {
                    app.exit(81); return;
                }
                window->grabWindow().save(directory + "/closed-document-ui.png");
                if (!clickCoverUi(window, "coverExportButton")) { app.exit(82); return; }
                QTimer::singleShot(800, &app, [window, &app, &session, &document, outputs, before, directory,
                                             layoutBefore, outputBefore, inputsCleared] {
                    if (QDir(directory).entryList({"card*.jpg", "card*.png"}, QDir::Files).size() != 3) {
                        app.exit(83); return;
                    }
                    if (!document.workspace().openSource(QStringLiteral("&title=Empty cover document\n&artist=\n&first=0\n"),
                        directory + "/empty/maidata.txt").accepted || !inputsCleared()) { app.exit(84); return; }
                    window->setProperty("coverOpen", true);
                    QTimer::singleShot(500, &app, [window, &app, &session, &document, outputs, before, directory,
                                                 layoutBefore, outputBefore, inputsCleared] {
                        if (!inputsCleared()) { app.exit(85); return; }
                        window->grabWindow().save(directory + "/no-difficulty-ui.png");
                        if (!document.workspace().openSource(before.sourceText, before.filePath, before.activeDifficultyId).accepted) {
                            app.exit(86); return;
                        }
                        window->setProperty("coverOpen", true);
                        QTimer::singleShot(800, &app, [window, &app, &session, &document, outputs, before, directory,
                                                     layoutBefore, outputBefore] {
                            const bool restored = session.pageSessionActive() && session.chartFrameAvailable()
                                && session.selectedDifficultyId() == before.activeDifficultyId
                                && !session.jacketImage().isEmpty()
                                && session.trackOverrides().value(QStringLiteral("title")).toString() == document.workspace().document().title
                                && session.coverLayout()->toJson() == layoutBefore && session.outputDirectory() == outputBefore;
                            window->grabWindow().save(directory + "/restored-document-ui.png");
                            QSaveFile report(directory + "/verification.json");
                            if (!report.open(QIODevice::WriteOnly)) { app.exit(87); return; }
                            report.write(QJsonDocument(QJsonObject{{"success", restored}, {"outputs", *outputs},
                                {"workspaceUnchangedDuringExports", true}, {"dualFrameReopen", true},
                                {"lastDifficultyRemovedInputsCleared", true},
                                {"closedDocumentInputsCleared", true}, {"noDifficultyInputsCleared", true},
                                {"emptyExportRejected", true}, {"sameDifficultyDocumentRestored", restored},
                                {"source", "actual v2 CoverExportPage button + document lifecycle"}}).toJson());
                            if (!report.commit()) { app.exit(88); return; }
                            app.exit(restored ? 0 : 89);
                        });
                    });
                });
            });
        });
    });
    QTimer::singleShot(500, &app, [window] { window->setProperty("coverOpen", true); });
    QTimer::singleShot(1800, &app, [window, &session, &document, &app, directory] {
        if (!session.pageSessionActive() || !session.chartFrameAvailable()
            || session.selectedDifficultyId() != document.activeDifficulty()
            || !window->findChild<QQuickItem*>("mobileCoverPage")) { app.exit(73); return; }
        session.setOutputDirectory(directory);
        session.coverLayout()->resetLayout();
        session.setBackgroundMode(static_cast<int>(cover_export::CoverBackgroundMode::Jacket));
        session.setResolutionIndex(0);
        session.setBlurBackground(false);
        session.addTextLayer();
        session.setActiveLayerText(QStringLiteral("MiaCode Android"));
        session.addChartFrameLayer();
        session.setActiveLayerFrameSeconds(10);
        window->grabWindow().save(directory + "/cover-ui.png");
        if (!clickCoverUi(window, "coverExportButton")) app.exit(74);
    });
    QTimer::singleShot(30000, &app, [&app] { app.exit(75); });
}
}
