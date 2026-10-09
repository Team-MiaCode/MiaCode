#pragma once
#include "MobileExportComposition.h"
#include "common/ProjectPreferences.h"
#include <QSaveFile>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QSettings>
#include <QDir>
#include <QDebug>
#include <memory>
#include <cstdio>

namespace miacode::android {
inline QQuickItem* settingsControlInVisualTree(QQuickItem* parent, const QString& name) {
    if (!parent) return nullptr;
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems())
        if (auto* result = settingsControlInVisualTree(child, name)) return result;
    return nullptr;
}
inline void startSettingsUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, MobilePreview& preview, MobileExportComposition& composition) {
    struct Proof {
        QElapsedTimer elapsed;
        qint64 phaseAt = 0;
        int phase = 0;
        int auditions = 0;
        double auditionGain = 0;
        PreviewAudioSettings audio;
        QVariantMap render;
        QVariantMap savedMobilePreferences;
        QByteArray projectPreferences;
        bool hadProjectPreferences = false;
        QString projectPreferencesPath;
    };
    auto proof = std::make_shared<Proof>();
    proof->audio = composition.audioSettings(); proof->render = composition.renderSettings();
    QSettings preferences;
    preferences.beginGroup("mobile");
    for (const auto& key : preferences.allKeys()) proof->savedMobilePreferences.insert(key, preferences.value(key));
    preferences.endGroup();
    proof->projectPreferencesPath = project_preferences::projectPreferencesFilePath(document.currentFilePath());
    QFile projectPreferences(proof->projectPreferencesPath);
    proof->hadProjectPreferences = projectPreferences.exists();
    if (proof->hadProjectPreferences && !projectPreferences.open(QIODevice::ReadOnly)) { app.exit(54); return; }
    if (proof->hadProjectPreferences)
        proof->projectPreferences = projectPreferences.readAll();
    projectPreferences.close();
    proof->elapsed.start();
    auto* timer = new QTimer(&app);
    timer->setInterval(50);
    auto* audition = composition.findChild<MobileAudioAudition*>();
    if (!audition) { app.exit(50); return; }
    QObject::connect(audition, &MobileAudioAudition::started, timer, [proof](const QString& kind, double gain) {
        if (kind == "judge") { ++proof->auditions; proof->auditionGain = gain; }
    });
    QObject::connect(timer, &QTimer::timeout, timer, [&, timer, proof, audition] {
        auto finish = [&](bool passed, const char* reason) {
            timer->stop();
            preview.setPlaying(false);
            composition.applyAudioSettings(proof->audio);
            for (const auto& key : {"brightnessOuter", "showTimestamp", "tapFlowSpeed"})
                composition.setRenderSetting(key, proof->render.value(key));
            QSettings defaults;
            defaults.beginGroup("mobile"); defaults.remove(QString());
            for (auto it = proof->savedMobilePreferences.cbegin(); it != proof->savedMobilePreferences.cend(); ++it)
                defaults.setValue(it.key(), it.value());
            defaults.endGroup();
            defaults.sync();
            const QString path = proof->projectPreferencesPath;
            bool restored = true;
            if (proof->hadProjectPreferences) {
                QSaveFile file(path);
                restored = file.open(QIODevice::WriteOnly) && file.write(proof->projectPreferences) == proof->projectPreferences.size() && file.commit();
            } else if (QFileInfo::exists(path)) restored = QFile::remove(path);
            passed &= restored && defaults.status() == QSettings::NoError;
            auto* model = qobject_cast<ui::AudioSettingsModel*>(composition.audioSettingsModel());
            model->releaseAudition();
            qInfo("Settings UI: %s; phase=%d; %s", passed ? "passed" : "failed", proof->phase, reason);
            std::fprintf(stderr, "Settings UI: %s; phase=%d; %s\n", passed ? "passed" : "failed", proof->phase, reason);
            app.exit(passed ? 0 : 51);
        };
        if (proof->elapsed.elapsed() > 20000) { finish(false, "timed out"); return; }
        if (engine.rootObjects().isEmpty()) return;
        auto* root = engine.rootObjects().first();
        auto* audio = root->findChild<QObject*>("shellAudioSettingsDialog");
        auto* render = root->findChild<QObject*>("shellPreviewSettingsDialog");
        auto* toolbar = root->findChild<QObject*>("mobileMainToolbar");
        auto* model = qobject_cast<ui::AudioSettingsModel*>(composition.audioSettingsModel());
        auto next = [&] { ++proof->phase; proof->phaseAt = proof->elapsed.elapsed(); };
        auto control = [&](QObject* popup, const char* name) -> QObject* {
            if (auto* object = popup->findChild<QObject*>(name)) return object;
            return settingsControlInVisualTree(popup->property("contentItem").value<QQuickItem*>(), name);
        };
        auto capture = [&](const char* name) {
            const QString directory = QFileInfo(document.currentFilePath()).absolutePath() + "/settings-ui-proof";
            QDir().mkpath(directory);
            auto* window = qobject_cast<QQuickWindow*>(root);
            return window && window->grabWindow().save(directory + '/' + name + ".png");
        };
        auto changeTap = [&](int percent) {
            auto* slider = control(audio, "audioLevel_tap");
            const bool held = slider && slider->setProperty("pressed", true);
            const bool value = slider && slider->setProperty("value", percent);
            const bool moved = slider && QMetaObject::invokeMethod(slider, "moved");
            std::fprintf(stderr, "Settings slider: found=%d held=%d write=%d moved=%d value=%g mix=%d\n",
                slider != nullptr, held, value, moved, slider ? slider->property("value").toDouble() : -1,
                composition.audioSettings().tapPercent());
            return held && value && moved;
        };
        auto press = [&](const char* name) {
            auto* item = control(audio, name);
            return item && QMetaObject::invokeMethod(item, "clicked");
        };
        switch (proof->phase) {
        case 0:
            if (proof->elapsed.elapsed() < 500) return;
            if (!audio || !render || !toolbar || !model
                || audio->property("audioSettings").value<QObject*>() != model
                || render->property("previewSettings").value<QObject*>() != composition.settings()
                || model->channels().size() != 10) { finish(false, "actual v2 dialog model binding"); return; }
            QMetaObject::invokeMethod(toolbar, "audioSettingsRequested"); next(); break;
        case 1:
            if (!audio->property("opened").toBool()) return;
            if (!capture("audio")) { finish(false, "audio capture failed"); return; }
            if (!changeTap(67) || composition.audioSettings().tapPercent() != 67
                || composition.buildSeedTask(document.activeDifficulty()).audioSettings.tapPercent() != 67) {
                finish(false, "slider did not reach preview/export mix"); return;
            }
            next(); break;
        case 2:
            if (proof->elapsed.elapsed() - proof->phaseAt < 350) return;
            if (proof->auditions) { finish(false, "audition fired while slider held"); return; }
            control(audio, "audioLevel_tap")->setProperty("pressed", false);
            next(); break;
        case 3:
            if (!proof->auditions) return;
            if (proof->auditions != 1 || qAbs(proof->auditionGain - previewSfxVolumeForKind(composition.audioSettings(), "judge")) > 0.001) {
                finish(false, "audition has wrong count or gain"); return;
            }
            if (!press("audioSaveDefaultButton") || !press("audioMute_tap") || !composition.audioSettings().tapMuted()
                || !press("audioMute_tap") || composition.audioSettings().tapPercent() != 67
                || !press("audioMute_global")) { finish(false, "mute restoration/default action"); return; }
            for (const auto& row : model->channels()) {
                if (!row.toMap().value("muted").toBool()) { finish(false, "master mute did not update every channel"); return; }
            }
            if (!press("audioMute_global") || composition.audioSettings().tapPercent() != 67
                || !changeTap(19) || !press("audioRestoreDefaultButton")
                || composition.audioSettings().tapPercent() != 67) { finish(false, "software default did not restore mix"); return; }
            control(audio, "audioLevel_tap")->setProperty("pressed", false);
            QMetaObject::invokeMethod(audio, "close");
            next(); break;
        case 4:
            if (audio->property("visible").toBool() || proof->elapsed.elapsed() - proof->phaseAt < 350) return;
            if (proof->auditions != 1 || !audition->findChildren<QSoundEffect*>().isEmpty()) {
                finish(false, "closed panel retained or replayed audition"); return;
            }
            QMetaObject::invokeMethod(toolbar, "previewSettingsRequested"); next(); break;
        case 5: {
            if (!render->property("opened").toBool()) return;
            if (!capture("video")) { finish(false, "video capture failed"); return; }
            auto* brightness = render->findChild<QObject*>("previewBrightnessOuterSlider");
            auto* timestamp = render->findChild<QObject*>("previewShowTimestampSwitch");
            if (!brightness || !timestamp || !QMetaObject::invokeMethod(brightness, "moved", Q_ARG(double, 37.0))) {
                finish(false, "video controls missing"); return;
            }
            timestamp->setProperty("checked", true);
            QMetaObject::invokeMethod(timestamp, "toggled");
            const auto task = composition.buildSeedTask(document.activeDifficulty());
            if (qAbs(preview.sceneRuntime().frameState().render.backgroundBrightnessOuter - 0.37) > 0.001
                || qAbs(task.backgroundBrightnessOuter - 0.37) > 0.001 || !task.showTimestamp) {
                finish(false, "video settings did not reach preview/export"); return;
            }
            render->setProperty("activePage", 1); next(); break;
        }
        case 6: {
            auto* speed = render->findChild<QQuickItem*>("previewTapFlowSpeedSlider");
            if (!speed || !speed->isVisible()) return;
            if (!capture("gameplay")) { finish(false, "gameplay capture failed"); return; }
            QMetaObject::invokeMethod(speed, "moved", Q_ARG(double, 6.0));
            if (qAbs(composition.buildSeedTask(document.activeDifficulty()).tapFlowSpeed - 6) > 0.001) {
                finish(false, "gameplay speed did not reach export"); return;
            }
            render->setProperty("activePage", 2); next(); break;
        }
        case 7:
            if (proof->elapsed.elapsed() - proof->phaseAt < 250) return;
            if (!capture("skin")) { finish(false, "skin capture failed"); return; }
            if (qobject_cast<ui::PreviewSettingsModel*>(composition.settings())->skinOptions().isEmpty()) {
                finish(false, "skin catalog is empty"); return;
            }
            QMetaObject::invokeMethod(render, "close");
            finish(true, "v2 dialogs, slider hold, audition gain, mute/default/close, preview/export settings"); break;
        }
    });
    timer->start();
}
}
