#pragma once

#include "SettingsUiSmoke.h"
#include "MobilePreferencesStore.h"
#include "app/ui/preferences/PreferencesModel.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include "app/ui/preferences/LocaleService.h"
#include "app/ui/editor/EditorController.h"
#include "app/ui/layout/WorkbenchSettings.h"
#include <QJsonArray>

namespace miacode::android {
inline bool preparePreferencesDialogUiSmoke(const QString& mode) {
    if (mode == "restart") return QFile::exists(PreferenceDocument::preferencesFilePath());
    if (mode != "seed" || QFile::exists(PreferenceDocument::preferencesFilePath())) return false;
    return PreferenceDocument::savePreferencesObject({
        {"ui", QJsonObject{{"language", "zh"}, {"theme", "dark"},
            {"editor_text_font_size", 16}, {"editor_ime_input_disabled", false}}},
        {"updates", QJsonObject{{"enabled", false}}},
        {"future_section", QJsonObject{{"sentinel", "preserved"}}}
    });
}
inline void startPreferencesDialogUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    ui::PreferencesModel& model, ui::WorkbenchSettings& settings, ui::EditorController& editor,
    MobilePreferencesStore& store, const QString& storage, const QString& mode) {
    struct Proof { QElapsedTimer elapsed; int phase = 0; bool resized = false; QStringList checks; };
    const auto proof = std::make_shared<Proof>(); proof->elapsed.start();
    auto* timer = new QTimer(&app); timer->setInterval(180);
    QObject::connect(timer, &QTimer::timeout, timer, [&, timer, proof, storage, mode] {
        auto finish = [&](bool ok, const QString& reason) {
            timer->stop();
            QSaveFile file(storage + "/preferences-dialog-" + mode + "-proof.json");
            const auto bytes = QJsonDocument(QJsonObject{{"ok", ok}, {"reason", reason},
                {"checks", QJsonArray::fromStringList(proof->checks)},
                {"usesActualV2Dialog", true}, {"usesActualPreferencesStore", true},
                {"controlHandlersVerified", ok}, {"nativeTouchVerified", false},
                {"P5Accepted", false}, {"pairedV2FidelityVerified", false}}).toJson(QJsonDocument::Indented);
            ok = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && ok;
            qInfo() << "Preferences dialog UI smoke:" << mode << (ok ? "passed" : "failed") << reason;
            app.exit(ok ? 0 : 62);
        };
        if (proof->elapsed.elapsed() > 20000) { finish(false, "deadline"); return; }
        if (engine.rootObjects().isEmpty()) return;
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window || !window->isVisible()) return;
        if (!proof->resized) {
            const auto args = app.arguments(); const int at = args.indexOf("--size");
            if (at >= 0) { const auto size = args.value(at + 1).split('x');
                if (size.size() != 2 || size[0].toInt() <= 0 || size[1].toInt() <= 0) { finish(false, "invalid geometry"); return; }
                window->resize(size[0].toInt(), size[1].toInt()); }
            proof->resized = true; return;
        }
        auto* dialog = window->findChild<QObject*>("mobilePreferencesDialog");
        auto* pane = window->findChild<QQuickItem*>("v2PreviewPane");
        auto* center = window->findChild<QQuickItem*>("mobileCenterSplit");
        auto control = [&](const char* name) {
            return dialog ? settingsControlInVisualTree(dialog->property("contentItem").value<QQuickItem*>(), QLatin1String(name)) : nullptr;
        };
        auto pick = [&](const char* name, const QVariant& value) {
            auto* item = control(name);
            return item && QMetaObject::invokeMethod(item, "picked", Q_ARG(QVariant, value));
        };
        auto capture = [&](const QString& name) { return window->grabWindow().save(storage + "/preferences-" + mode + '-' + name + ".png"); };
        if (!dialog || !pane || !center) { finish(false, "production objects missing"); return; }
        if (proof->phase == 0) {
            if (mode == "restart") {
                if (model.editorFontSize() != 20 || model.editorLineSpacing() != 2
                    || model.editorInputHandlingMode() != 0 || editor.imeInputDisabled()
                    || !editor.halfWidthInputEnabled() || editor.autoCompletionEnabled()
                    || model.canvasFrameRateMode() != 0 || model.stageMediaFrameRateMode() != 1
                    || model.timelineFrameRateMode() != 0 || !model.previewOnLeft()
                    || !model.videoDecodePrefersSoftware()) { finish(false, "cold restoration differs from saved preferences"); return; }
                proof->checks << "cold process restores combined input flags, appearance, three modes, decoder policy and preview side";
            }
            if (!QMetaObject::invokeMethod(dialog, "open")) { finish(false, "open actual dialog"); return; }
            proof->phase++; return;
        }
        if (!dialog->property("visible").toBool()) return;
        if (mode == "restart") {
            if (!(pane->x() < center->x()) || !capture("restored-left")) { finish(false, "restored preview side or capture"); return; }
            finish(true, "cold settings and actual left preview restored"); return;
        }
        switch (proof->phase) {
        case 1:
            if (!pick("preferencesLanguageCombo", "ja") || LocaleService::instance().activeLanguageToken() != "ja") { finish(false, "live Japanese translation"); return; }
            proof->phase++; break;
        case 2:
            if (!capture("interface-ja") || !pick("preferencesLanguageCombo", "zh")
                || !pick("preferencesPreviewSideCombo", true)) { finish(false, "interface controls"); return; }
            proof->phase++; break;
        case 3:
            if (!(pane->x() < center->x()) || !capture("interface-left")) { finish(false, "live preview swap"); return; }
            proof->checks << "live language catalog and QML retranslation; preview physically moves left";
            dialog->setProperty("activePage", 1); proof->phase++; break;
        case 4:
            if (!control("preferencesBackgroundOpacitySlider") || !capture("background")) { finish(false, "actual background page"); return; }
            dialog->setProperty("activePage", 2); proof->phase++; break;
        case 5: {
            auto* font = control("preferencesFontSizeSlider");
            if (!font || !QMetaObject::invokeMethod(font, "moved", Q_ARG(double, 20.0))
                || !pick("preferencesLineSpacingCombo", 2.0)
                || !pick("preferencesInputHandlingCombo", 2)) { finish(false, "editor handlers"); return; }
            if (editor.imeInputDisabled() || editor.halfWidthInputEnabled()) { finish(false, "unchanged input mode consumers"); return; }
            if (!pick("preferencesInputHandlingCombo", 1) || !editor.imeInputDisabled() || !editor.halfWidthInputEnabled()
                || !pick("preferencesInputHandlingCombo", 0) || editor.imeInputDisabled() || !editor.halfWidthInputEnabled()) {
                finish(false, "combined input flags lost by nonpersistent first setter"); return;
            }
            auto* completion = control("preferencesAutoCompletionSwitch");
            if (!completion || !completion->setProperty("checked", false) || !QMetaObject::invokeMethod(completion, "toggled")) { finish(false, "completion handler"); return; }
            proof->phase++; break;
        }
        case 6: {
            auto* area = settingsControlInVisualTree(window->contentItem(), "sourceArea");
            if (!area || settings.codeFont().pointSize() != 20 || area->property("font").value<QFont>() != settings.renderedCodeFont()
                || settings.editorBlockSpacing() != 8 || editor.autoCompletionEnabled()
                || !capture("editor")) { finish(false, "editor consumers"); return; }
            proof->checks << "v2 slider and combo handlers update real font, spacing, completion and all input modes";
            dialog->setProperty("activePage", 3); proof->phase++; break;
        }
        case 7:
            if (!pick("preferencesCanvasFrameRateCombo", 0)
                || store.timelineFrameRateMode() != PreviewCanvasFrameRateMode::Fps60
                || PreferenceDocument::loadPreferencesObject().value("preview").toObject()
                    .value("timeline_frame_rate_mode").toString() != "60") {
                finish(false, "canvas change must preserve an initially implicit timeline rate for cold start"); return;
            }
            proof->checks << "changing canvas preserves independent implicit timeline rate in saved preferences";
            if (!pick("preferencesPvFrameRateCombo", 1)
                || !pick("preferencesTimelineFrameRateCombo", 0) || !pick("preferencesVideoDecodeCombo", true)
                || !model.decoderRestartRequired() || store.previewCanvasFrameRateMode() != PreviewCanvasFrameRateMode::Fps30
                || store.previewStageMediaFrameRateMode() != PreviewCanvasFrameRateMode::Fps60
                || store.timelineFrameRateMode() != PreviewCanvasFrameRateMode::Fps30) { finish(false, "performance handlers"); return; }
            proof->phase++; break;
        case 8:
            if (!control("preferencesDecoderRestartHint") || !control("preferencesDecoderRestartHint")->isVisible() || !capture("performance")) { finish(false, "decoder restart hint"); return; }
            proof->checks << "three independent frame-rate modes and explicit decoder cold-start requirement";
            dialog->setProperty("activePage", 4); proof->phase++; break;
        case 9:
            if (!control("shortcutList") || control("shortcutList")->property("count").toInt() <= 0 || !capture("shortcuts")) { finish(false, "actual shortcut registry page"); return; }
            dialog->setProperty("activePage", 5); proof->phase++; break;
        case 10: {
            if (!dialog->property("updateService").value<QObject*>() || !capture("updates")) { finish(false, "actual update service page"); return; }
            const auto root = PreferenceDocument::loadPreferencesObject(); const auto ui = root.value("ui").toObject();
            if (!ui.value("editor_half_width_input").toBool() || ui.value("editor_ime_input_disabled").toBool(true)
                || root.value("future_section").toObject().value("sentinel") != "preserved") { finish(false, "atomic flags and unknown fields"); return; }
            proof->checks << "six actual v2 pages render; real update service injected; unrelated preference fields preserved";
            finish(true, "six pages, live consumers and saved preference document verified"); break;
        }
        }
    });
    timer->start();
}
}
