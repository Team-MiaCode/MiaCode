#pragma once
#include "SettingsUiSmoke.h"
#include "app/ui/preferences/AppBackgroundModel.h"
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>

namespace miacode::android {
// Actual QML page and production model, isolated by main's diagnostic app name.
// The image is a deterministic QA fixture; this does not claim a SAF-picker test.
inline void startBackgroundUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, ui::AppBackgroundModel& background,
    const QString& storage, const QString& mode) {
    struct Proof { QElapsedTimer elapsed; int phase = 0; qint64 phaseAt = 0; QStringList checks; };
    auto proof = std::make_shared<Proof>();
    proof->elapsed.start();
    auto* timer = new QTimer(&app);
    timer->setInterval(50);
    QObject::connect(timer, &QTimer::timeout, timer, [&, timer, proof, storage, mode] {
        auto finish = [&](bool ok, const QString& reason) {
            timer->stop();
            QJsonObject report{{"passed", ok}, {"reason", reason}, {"mode", mode},
                {"checks", QJsonArray::fromStringList(proof->checks)},
                {"background", QJsonObject{{"enabled", background.enabled()},
                    {"path", background.imagePath()}, {"readable", background.imageReadable()},
                    {"opacity", background.opacity()}, {"blur", background.blur()},
                    {"panelAlpha", background.panelAlpha()}, {"sizeMode", background.sizeMode()},
                    {"position", background.position()}}},
                {"P5Accepted", false}, {"pairedV2FidelityVerified", false},
                {"SAFPickerVerified", false}};
            QSaveFile file(storage + "/background-" + mode + "-proof.json");
            if (!file.open(QIODevice::WriteOnly)) ok = false;
            else {
                const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
                ok = file.write(bytes) == bytes.size() && file.commit() && ok;
            }
            qInfo() << "Background UI smoke:" << (ok ? "passed" : "failed") << mode << reason;
            app.exit(ok ? 0 : 59);
        };
        if (proof->elapsed.elapsed() > 18000) { finish(false, "deadline"); return; }
        if (engine.rootObjects().isEmpty()) return;
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window || !window->isExposed()) return;
        auto* image = settingsControlInVisualTree(window->contentItem(), "applicationBackgroundImage");
        auto* dialog = window->findChild<QObject*>("mobilePreferencesDialog");
        if (!image || !dialog) { finish(false, "missing actual QML background or dialog"); return; }
        auto capture = [&](const QString& name) { return window->grabWindow().save(storage + "/" + name + ".png"); };
        const QString fixture = QDir::cleanPath(storage + "/background-fixture.png");
        if (proof->phase == 0) {
            const int sizeIndex = app.arguments().indexOf("--size");
            if (sizeIndex >= 0) {
                const auto size = app.arguments().value(sizeIndex + 1).split('x');
                if (size.size() != 2 || size[0].toInt() <= 0 || size[1].toInt() <= 0) {
                    finish(false, "invalid capture size"); return;
                }
                window->resize(size[0].toInt(), size[1].toInt());
            }
            if (mode == "seed") {
                QImage pixels(640, 360, QImage::Format_RGB32);
                for (int y = 0; y < pixels.height(); ++y)
                    for (int x = 0; x < pixels.width(); ++x)
                        pixels.setPixelColor(x, y, QColor(40 + x * 180 / 640, 30 + y * 140 / 360, (x / 80 + y / 60) % 2 ? 180 : 60));
                if (!pixels.save(fixture)) { finish(false, "fixture save failed"); return; }
                background.setImagePath(fixture);
                background.setEnabled(true);
                background.setOpacity(0.5);
                background.setBlur(0);
                background.setPanelAlpha(120);
                QMetaObject::invokeMethod(dialog, "open");
                dialog->setProperty("activePage", 1);
                proof->phase = 1; proof->phaseAt = proof->elapsed.elapsed(); return;
            }
            if (mode != "restart") { finish(false, "unknown mode"); return; }
            if (!background.enabled() || !background.imageReadable() || background.imagePath() != fixture
                || qAbs(background.opacity() - 0.63) > 0.0001 || background.blur() != 4
                || background.panelAlpha() != 80 || background.sizeMode() != "contain"
                || background.position() != "right_top") {
                finish(false, "cold restart lost background values"); return;
            }
            proof->checks << "cold process restored all background fields";
            proof->phase = 3; proof->phaseAt = proof->elapsed.elapsed(); return;
        }
        if (proof->elapsed.elapsed() - proof->phaseAt < 900) return;
        if (proof->phase == 1) {
            auto* opacity = settingsControlInVisualTree(window->contentItem(), "preferencesBackgroundOpacitySlider");
            auto* panel = settingsControlInVisualTree(window->contentItem(), "preferencesBackgroundPanelSlider");
            auto* blur = settingsControlInVisualTree(window->contentItem(), "preferencesBackgroundBlurSlider");
            auto* size = settingsControlInVisualTree(window->contentItem(), "preferencesBackgroundScaleCombo");
            auto* position = settingsControlInVisualTree(window->contentItem(), "preferencesBackgroundPositionCombo");
            if (!opacity || !panel || !blur || !size || !position
                || !QMetaObject::invokeMethod(opacity, "moved", Q_ARG(double, 0.63))
                || !QMetaObject::invokeMethod(panel, "moved", Q_ARG(double, 80.0 / 255.0))
                || !QMetaObject::invokeMethod(blur, "moved", Q_ARG(double, 4.0))
                || !QMetaObject::invokeMethod(size, "picked", Q_ARG(QVariant, QVariant("contain")))
                || !QMetaObject::invokeMethod(position, "picked", Q_ARG(QVariant, QVariant("right_top")))) {
                finish(false, "shared v2 control dispatch failed"); return;
            }
            proof->checks << "shared v2 sliders and combos dispatched their actual write handlers";
            proof->phase = 2; proof->phaseAt = proof->elapsed.elapsed(); return;
        }
        if (proof->phase == 2) {
            if (background.opacity() != 0.63 || background.panelAlpha() != 80 || background.blur() != 4
                || background.sizeMode() != "contain" || background.position() != "right_top"
                || !capture("background-settings")) { finish(false, "control values or settings capture failed"); return; }
            QMetaObject::invokeMethod(dialog, "close");
            proof->phase = 3; proof->phaseAt = proof->elapsed.elapsed(); return;
        }
        if (proof->phase == 3) {
            if (image->property("status").toInt() != 1 || !image->isVisible()
                || qAbs(image->opacity() - 0.63) > 0.0001 || image->property("fillMode").toInt() != 1
                || !capture("background-" + mode + "-on")) { finish(false, "actual image render or capture failed"); return; }
            const QString label = document.currentDifficultyLabel();
            const auto rows = document.difficulties();
            bool hasLabel = false;
            for (const auto& row : rows) if (row.toMap().value("id").toInt() == document.activeDifficulty())
                hasLabel = row.toMap().value("label").toString().trimmed() == label;
            if (!hasLabel || !window->property("documentTitle").toString().endsWith(label)) {
                finish(false, "title or list projection lost level"); return;
            }
            proof->checks << "actual Image ready with configured fill/opacity and no settings overlay"
                          << "title and difficulty list include current level";
            if (mode == "seed") { finish(true, "persisted page values for cold restart"); return; }
            background.setEnabled(false);
            proof->phase = 4; proof->phaseAt = proof->elapsed.elapsed(); return;
        }
        if (proof->phase == 4) {
            if (image->isVisible() || !capture("background-restart-off")) { finish(false, "disabled wallpaper still visible"); return; }
            background.setEnabled(true);
            background.setImagePath(storage + "/missing-background.png");
            if (background.imagePath() != fixture || background.errorMessage().isEmpty()) {
                finish(false, "invalid image replaced valid selection"); return;
            }
            background.clearImage();
            if (background.imageReadable() || !background.sourceUrl().isEmpty()) { finish(false, "clear left readable wallpaper"); return; }
            proof->checks << "disable hides actual wallpaper" << "missing file preserves prior selection and reports error"
                          << "clear removes readable source";
            finish(true, "restart and wallpaper gates verified");
        }
    });
    timer->start();
}
}
