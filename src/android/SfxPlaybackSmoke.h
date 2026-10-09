#pragma once
#include "MobileExportComposition.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <memory>
#include <cstdio>

namespace miacode::android {
inline void startSfxPlaybackSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, MobilePreview& preview, MobileExportComposition& composition) {
    struct Proof {
        QElapsedTimer elapsed;
        qint64 phaseAt = 0;
        int phase = 0;
        quint64 pausedFrames = 0;
        PreviewAudioSettings audio;
        QJsonArray observations;
    };
    auto proof = std::make_shared<Proof>(); proof->elapsed.start(); proof->audio = composition.audioSettings();
    auto* timer = new QTimer(&app); timer->setInterval(50);
    QObject::connect(timer, &QTimer::timeout, timer, [&, timer, proof] {
        const auto state = preview.sfxDiagnostics();
        const auto finish = [&](bool passed, const QString& reason) {
            proof->observations.append(QJsonObject{{"phase", proof->phase}, {"output", QJsonObject::fromVariantMap(state)},
                {"position", preview.positionSeconds()}, {"playing", preview.playing()}, {"final", true}});
            timer->stop(); preview.stop(); composition.applyAudioSettings(proof->audio);
            const QString directory = QFileInfo(document.currentFilePath()).absolutePath();
            QSaveFile output(directory + "/sfx-playback-proof.json");
            const QByteArray bytes = QJsonDocument(QJsonObject{{"passed", passed}, {"phase", proof->phase}, {"reason", reason},
                {"observations", proof->observations}, {"P5Accepted", false}, {"pairedV2FidelityVerified", false}}).toJson(QJsonDocument::Indented);
            const bool saved = output.open(QIODevice::WriteOnly) && output.write(bytes) == bytes.size() && output.commit();
            std::fprintf(stderr, "SFX playback: %s; phase=%d; %s\n", passed && saved ? "passed" : "failed", proof->phase, qPrintable(reason));
            qInfo("SFX playback: %s; phase=%d; %s", passed && saved ? "passed" : "failed", proof->phase, qPrintable(reason));
            app.exit(passed && saved ? 0 : 55);
        };
        const auto next = [&] {
            proof->observations.append(QJsonObject{{"phase", proof->phase}, {"output", QJsonObject::fromVariantMap(preview.sfxDiagnostics())},
                {"position", preview.positionSeconds()}, {"playing", preview.playing()}});
            ++proof->phase; proof->phaseAt = proof->elapsed.elapsed();
        };
        if (proof->elapsed.elapsed() > 25000) { finish(false, "deadline exceeded"); return; }
        switch (proof->phase) {
        case 0: {
            if (engine.rootObjects().isEmpty() || !state.value("ready").toBool()) return;
            auto* pane = engine.rootObjects().first()->findChild<QObject*>("v2PreviewPane");
            if (!pane || pane->property("previewSession").value<QObject*>() != &preview) {
                finish(false, "actual v2 preview pane is not bound to transport"); return;
            }
            document.setMetadataFirst("0");
            document.setChartText("(120){4}1,2,3,4,1b,2x,C1h[4:4],C1f,3-7[4:2],E");
            auto levels = proof->audio;
            levels.globalVolume = levels.tapVolume = 1; levels.trackVolume = levels.answerVolume = 0;
            levels.exVolume = levels.breakVolume = levels.breakSlideVolume = levels.slideVolume = levels.touchVolume = levels.fireworkVolume = 0;
            composition.applyAudioSettings(levels); preview.setPositionSeconds(0); preview.setPlaying(true); next(); break;
        }
        case 1:
            if (proof->elapsed.elapsed() - proof->phaseAt < 1200) return;
            if (!preview.playing() || !state.value("running").toBool()
                || state.value("renderedFrames").toULongLong() < quint64(state.value("sampleRate").toInt() / 2)
                || state.value("nonzeroFrames").toULongLong() < 100) { finish(false, "real audio sink did not consume note PCM"); return; }
            preview.setPlaying(false); next(); break;
        case 2:
            if (proof->elapsed.elapsed() - proof->phaseAt < 250) return;
            if (state.value("running").toBool() || !state.value("suspended").toBool()) { finish(false, "pause did not suspend output"); return; }
            proof->pausedFrames = state.value("renderedFrames").toULongLong(); next(); break;
        case 3:
            if (proof->elapsed.elapsed() - proof->phaseAt < 300) return;
            if (state.value("renderedFrames").toULongLong() != proof->pausedFrames) { finish(false, "paused output keeps rendering"); return; }
            preview.setPlaying(true); next(); break;
        case 4: {
            if (proof->elapsed.elapsed() - proof->phaseAt < 250) return;
            if (!state.value("running").toBool() || state.value("renderedFrames").toULongLong() <= proof->pausedFrames) {
                finish(false, "resume did not resume retained output"); return;
            }
            preview.beginScrub(); preview.updateScrub(3.4);
            if (preview.playing() || preview.sfxDiagnostics().value("running").toBool()) { finish(false, "scrub did not silence output"); return; }
            auto levels = composition.audioSettings(); levels.tapVolume = 0; levels.touchVolume = 1;
            composition.applyAudioSettings(levels); preview.endScrub();
            if (preview.playing() || preview.sfxDiagnostics().value("running").toBool()) {
                finish(false, "scrub release must remain paused until explicit play"); return;
            }
            preview.setPlaying(true); next(); break;
        }
        case 5:
            if (proof->elapsed.elapsed() - proof->phaseAt < 180) return;
            if (!preview.playing() || !state.value("running").toBool() || state.value("nonzeroFrames").toULongLong() < 100) {
                finish(false, "explicit play after scrub failed to restore active touch-hold PCM"); return;
            }
            document.addDifficulty(6); document.selectDifficulty(6); document.setChartText("(120){4},,,,,,,,,,,,,E"); next(); break;
        case 6:
            if (proof->elapsed.elapsed() - proof->phaseAt < 500) return;
            if (!state.value("running").toBool() || state.value("renderedFrames").toULongLong() < 1000
                || state.value("nonzeroFrames").toULongLong() != 0) { finish(false, "old difficulty voices survived replacement"); return; }
            preview.setPlaying(false); preview.setRate(2); preview.setPositionSeconds(0); preview.setPlaying(true); next(); break;
        case 7:
            if (proof->elapsed.elapsed() - proof->phaseAt < 300) return;
            if (!state.value("running").toBool() || qAbs(preview.rate() - 2) > .001) { finish(false, "2x output did not start"); return; }
            next(); finish(true, "actual v2 pane binding, device PCM, pause/resume, scrub stays paused then explicit play restores hold, difficulty isolation and rate change"); break;
        }
    });
    timer->start();
}
}
