#pragma once

#include "SettingsUiSmoke.h"
#include "MobileTimeline.h"
#include "app/ui/editor/EditorController.h"
#include "app/ui/editor/EditorTextStyle.h"
#include "app/ui/layout/WorkbenchSettings.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include <QJsonArray>
#include <QTextBlock>
#include <QTextDocument>

namespace miacode::android {
inline bool prepareWorkbenchUiSmoke(const QString& mode)
{
    if (mode == "restart") return QFile::exists(PreferenceDocument::preferencesFilePath());
    if (mode != "seed" || QFile::exists(PreferenceDocument::preferencesFilePath())) return false;
    return PreferenceDocument::savePreferencesObject(QJsonObject{
        {"ui", QJsonObject{{"theme", "dark"}, {"editor_text_font_size", 16},
            {"editor_line_spacing_factor", 3.0}, {"editor_half_width_input", false},
            {"editor_ime_input_disabled", false}, {"editor_auto_completion", false}}},
        {"app", QJsonObject{{"preview", QJsonObject{{"timeline_zoom_scale", 3.0}, {"timeline_waveform_brightness", 0.72},
            {"timeline_measure_line_brightness", 0.43}, {"follow_preview", true},
            {"viewport_lock", false}, {"follow_progress", false}, {"future_timeline_setting", "preserved"}}},
            {"future_app_setting", "preserved"}}},
        {"preview", QJsonObject{{"future_frame_rate_setting", "preserved"}}},
        {"future_section", QJsonObject{{"sentinel", "preserved"}}}
    });
}

inline void startWorkbenchUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    ui::WorkbenchSettings& settings, ui::EditorController& controller, MobilePreview& preview, MobileTimeline& timeline,
    const QString& storage, const QString& mode)
{
    struct Proof {
        QElapsedTimer elapsed; int phase = 0; bool resized = false; QStringList checks;
        int calibrationPhase = 0;
        QString originalPreviewChart;
    };
    const auto proof = std::make_shared<Proof>();
    proof->elapsed.start();
    auto* timer = new QTimer(&app);
    timer->setInterval(100);
    QObject::connect(timer, &QTimer::timeout, timer, [&, timer, proof, storage, mode] {
        auto finish = [&](bool ok, const QString& reason) {
            timer->stop();
            QSaveFile file(storage + "/workbench-" + mode + "-proof.json");
            const auto bytes = QJsonDocument(QJsonObject{{"ok", ok}, {"reason", reason},
                {"checks", QJsonArray::fromStringList(proof->checks)},
                {"pointSize", settings.codeFont().pointSize()}, {"fontFamily", settings.codeFont().family()},
                {"blockSpacing", settings.editorBlockSpacing()}, {"theme", settings.themeModeToken()},
                {"timelineViewSettingsVerified", ok}, {"nativeTouchVerified", false},
                {"P5Accepted", false}, {"pairedV2FidelityVerified", false}}).toJson(QJsonDocument::Indented);
            ok = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && ok;
            qInfo() << "Workbench UI smoke:" << mode << (ok ? "passed" : "failed") << reason;
            app.exit(ok ? 0 : 60);
        };
        if (proof->elapsed.elapsed() > 15000) { finish(false, "deadline"); return; }
        if (engine.rootObjects().isEmpty()) return;
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window || !window->isExposed() || proof->elapsed.elapsed() < 1200) return;
        if (!proof->resized) {
            const int index = app.arguments().indexOf("--size");
            if (index >= 0) {
                const auto size = app.arguments().value(index + 1).split('x');
                if (size.size() != 2 || size[0].toInt() <= 0 || size[1].toInt() <= 0) {
                    finish(false, "invalid geometry"); return;
                }
                window->resize(size[0].toInt(), size[1].toInt());
            }
            proof->resized = true;
            return;
        }
        auto* pages = window->findChild<QObject*>("mobilePages");
        if (!pages) { finish(false, "actual page owner missing"); return; }
        if (proof->calibrationPhase == 0) {
            proof->originalPreviewChart = preview.previewChartText();
            if (!QMetaObject::invokeMethod(pages, "openLatencyPage")) { finish(false, "open actual calibration"); return; }
            proof->calibrationPhase = 1;
            return;
        }
        if (proof->calibrationPhase == 1) {
            auto* field = settingsControlInVisualTree(window->contentItem(), "latencyBpmField");
            if (!preview.latencyChartActive() || !field || pages->property("activePageId").toString() != "latency"
                || preview.previewChartText() == proof->originalPreviewChart) {
                finish(false, "calibration page did not enter the actual audition source"); return;
            }
            if (!QMetaObject::invokeMethod(pages, "activateMetadataPage")) { finish(false, "leave actual calibration"); return; }
            proof->calibrationPhase = 2;
            return;
        }
        if (proof->calibrationPhase == 2) {
            if (preview.latencyChartActive() || preview.playing()
                || preview.previewChartText() != proof->originalPreviewChart) {
                finish(false, "calibration exit did not restore actual chart preview"); return;
            }
            if (!QMetaObject::invokeMethod(pages, "activateDifficultyPage")) { finish(false, "return actual difficulty"); return; }
            proof->calibrationPhase = 3;
            proof->checks.append("actual calibration page enters sandbox and restores difficulty");
            return;
        }
        auto* area = settingsControlInVisualTree(window->contentItem(), "sourceArea");
        if (!area) { finish(false, "actual source editor missing"); return; }
        const int size = mode == "seed" && proof->phase == 0 ? 16 : 20;
        const int spacing = size == 16 ? 9 : 6;
        auto* bridge = qobject_cast<TimelineQuickStateBridge*>(timeline.stateBridge());
        auto* bottom = window->findChild<QObject*>("mobileBottomPanel");
        const bool initialTimeline = mode == "seed" && proof->phase == 0;
        if (!bridge || !bottom || bottom->property("timelineSession").value<QObject*>() != &timeline
            || !qFuzzyCompare(bridge->zoomScale(), initialTimeline ? 3.0 : 1.5)
            || !qFuzzyCompare(bridge->waveformBrightness(), initialTimeline ? 0.72 : 0.36)
            || !qFuzzyCompare(bridge->measureLineBrightness(), initialTimeline ? 0.43 : 0.64)
            || !bridge->viewportLockEnabled() || !bridge->followProgressEnabled()
            || bridge->followPreviewEnabled() != initialTimeline) {
            finish(false, "actual timeline consumer did not restore view settings"); return;
        }
        if (area->property("font").value<QFont>() != settings.renderedCodeFont()
            || settings.codeFont().pointSize() != size || settings.editorBlockSpacing() != spacing) {
            finish(false, "actual QML editor appearance differs from preferences"); return;
        }
        const auto styles = window->findChildren<ui::EditorTextStyle*>();
        bool styleMatches = false;
        for (auto* style : styles) {
            auto* doc = style->textDocument() ? style->textDocument()->textDocument() : nullptr;
            if (style->blockSpacing() == spacing && doc && !doc->isEmpty()
                && doc->firstBlock().blockFormat().lineHeightType() == QTextBlockFormat::LineDistanceHeight
                && qFuzzyCompare(doc->firstBlock().blockFormat().lineHeight(), double(spacing)))
                styleMatches = true;
        }
        if (!styleMatches) { finish(false, "actual text document spacing differs"); return; }
        if (size == 16) {
            if (controller.halfWidthInputEnabled() || controller.imeInputDisabled()
                || controller.autoCompletionEnabled()) { finish(false, "restored input settings not applied"); return; }
            proof->checks << "initial actual QML font, document spacing and input settings";
            proof->checks << "initial actual timeline consumer restores zoom, brightness and code follow";
            if (!window->grabWindow().save(storage + "/workbench-before.png")) {
                finish(false, "initial capture"); return;
            }
            auto root = PreferenceDocument::loadPreferencesObject();
            auto ui = root.value("ui").toObject();
            ui.insert("editor_text_font_size", 20);
            ui.insert("editor_line_spacing_factor", 1.5);
            ui.insert("editor_half_width_input", true);
            ui.insert("editor_ime_input_disabled", true);
            ui.insert("editor_auto_completion", true);
            root.insert("ui", ui);
            if (!PreferenceDocument::savePreferencesObject(root)) { finish(false, "save editor settings"); return; }
            settings.reloadEditorSettings();
            settings.setEditorScrollPastEnd(false);
            settings.setEditorSelectionBeatDisplay(false);
            settings.setPreviewHidePv(true);
            settings.setPreviewCanvasFreeAspect(true);
            bridge->applyZoomPreset(1.5);
            bridge->setWaveformBrightness(0.36);
            bridge->setMeasureLineBrightness(0.64);
            timeline.followPreviewToggled(false);
            auto* dialog = window->findChild<QObject*>("mobilePreferencesDialog");
            if (!dialog || !QMetaObject::invokeMethod(dialog, "open")) { finish(false, "open actual settings"); return; }
            proof->phase = 1;
            return;
        }
        if (!controller.halfWidthInputEnabled() || !controller.imeInputDisabled()
            || !controller.autoCompletionEnabled() || settings.editorScrollPastEnd()
            || settings.editorSelectionBeatDisplay() || !settings.previewHidePv()
            || preview.hidePv() != settings.previewHidePv()
            || !settings.previewCanvasFreeAspect()) { finish(false, "changed or restored settings not applied"); return; }
        if (mode == "seed" && proof->phase == 1) {
            auto* combo = settingsControlInVisualTree(window->contentItem(), "preferencesThemeModeCombo");
            if (!combo) {
                auto* dialog = window->findChild<QObject*>("mobilePreferencesDialog");
                combo = settingsControlInVisualTree(qobject_cast<QQuickItem*>(dialog->property("contentItem").value<QObject*>()), "preferencesThemeModeCombo");
            }
            if (!combo || !QMetaObject::invokeMethod(combo, "picked", Q_ARG(QVariant, QVariant("light")))) { finish(false, "actual v2 theme combo handler"); return; }
            auto* dialog = window->findChild<QObject*>("mobilePreferencesDialog");
            QMetaObject::invokeMethod(dialog, "close");
            proof->phase = 2;
            return;
        }
        const auto saved = PreferenceDocument::loadPreferencesObject();
        const auto appPreferences = saved.value("app").toObject();
        const auto timelinePreferences = appPreferences.value("preview").toObject();
        if (timelinePreferences.value("timeline_zoom_scale").toDouble() != 1.5
            || timelinePreferences.value("timeline_waveform_brightness").toDouble() != 0.36
            || timelinePreferences.value("timeline_measure_line_brightness").toDouble() != 0.64
            || !timelinePreferences.value("follow_preview").isBool()
            || timelinePreferences.value("follow_preview").toBool()
            || timelinePreferences.value("future_timeline_setting").toString() != "preserved"
            || appPreferences.value("future_app_setting").toString() != "preserved"
            || saved.value("preview").toObject().value("future_frame_rate_setting").toString() != "preserved") {
            finish(false, "timeline settings persistence or foreign preview field preservation"); return;
        }
        if (settings.darkTheme() || settings.themeModeToken() != "light"
            || PreferenceDocument::loadPreferencesObject().value("future_section").toObject()
                .value("sentinel").toString() != "preserved") {
            finish(false, "theme or unknown settings preservation"); return;
        }
        proof->checks << "actual QML font and spacing after change or cold restart"
            << "input flags, view settings, theme and foreign settings restored"
            << "actual timeline consumers and merged preferences survive changes and cold restart";
        if (!window->grabWindow().save(storage + "/workbench-" + mode + ".png")) {
            finish(false, "final capture"); return;
        }
        finish(true, "");
    });
    timer->start();
}
} // namespace miacode::android
