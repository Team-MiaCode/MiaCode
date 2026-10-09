#include "app/ui/layout/WorkbenchSettings.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include "app/ui/editor/EditorController.h"
#include "common/EditorAppearance.h"

#include <QGuiApplication>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QDebug>

using miacode::ui::WorkbenchSettings;
using miacode::ui::EditorController;

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("MiaCode");
    app.setApplicationName("MiaCodeWorkbenchSettingsSpec");
    QTemporaryDir storage;
    if (!storage.isValid()) return 1;
    PreferenceDocument::setPreferencesFilePath(storage.path() + "/preferences.json");
    const QJsonObject initial{
        {"ui", QJsonObject{{"editor_text_font_size", 16}, {"editor_line_spacing_factor", 3.0},
            {"editor_half_width_input", false}, {"editor_ime_input_disabled", false},
            {"editor_auto_completion", false}, {"editor_scroll_past_end", true},
            {"editor_selection_beat_display", true}, {"future_editor_setting", "preserved"}}},
        {"preview", QJsonObject{{"future_render_setting", 42}}},
        {"future_section", QJsonObject{{"sentinel", "preserved"}}}
    };
    if (!PreferenceDocument::savePreferencesObject(initial)) return 2;
    auto require = [](bool condition, const char* label) {
        if (!condition) qCritical() << label;
        return condition;
    };
    bool ok = true;
    {
        WorkbenchSettings settings;
        EditorController controller;
        const auto apply = [&] {
            controller.setHalfWidthInputEnabled(settings.editorHalfWidthInputEnabled());
            controller.setImeInputDisabled(settings.editorImeInputDisabled());
            controller.setAutoCompletionEnabled(settings.editorAutoCompletionEnabled());
            controller.setOverwriteMode(settings.editorOverwriteModeEnabled());
        };
        QObject::connect(&settings, &WorkbenchSettings::editorSettingsChanged, &controller, apply);
        apply();
        ok &= require(settings.codeFont().pointSize() == 16
            && settings.editorBlockSpacing() == 9, "stored point size and line spacing");
        ok &= require(settings.codeFont().family().contains("Maple", Qt::CaseInsensitive), "bundled v2 font resource");
        ok &= require(!controller.imeInputDisabled() && !controller.autoCompletionEnabled(), "initial controller settings");
        const QString fullWidth = QString(QChar(0xff11)) + QChar(0xff0c);
        ok &= require(controller.processKey("", 0, 0, fullWidth, Qt::Key_1, 0).transaction.text == fullWidth,
            "leave full-width input unchanged");

        auto root = PreferenceDocument::loadPreferencesObject();
        auto ui = root.value("ui").toObject();
        ui.insert("editor_text_font_size", 24);
        ui.insert("editor_line_spacing_factor", 2.0);
        ui.insert("editor_half_width_input", true);
        ui.insert("editor_ime_input_disabled", true);
        ui.insert("editor_auto_completion", true);
        root.insert("ui", ui);
        ok &= require(PreferenceDocument::savePreferencesObject(root), "atomic settings update");
        settings.reloadEditorSettings();
        ok &= require(settings.codeFont().pointSize() == 24 && settings.editorBlockSpacing() == 8,
            "editor appearance reload");
        ok &= require(controller.imeInputDisabled() && controller.autoCompletionEnabled(),
            "controller receives changed settings");
        ok &= require(controller.processKey("", 0, 0, fullWidth, Qt::Key_1, 0).transaction.text == "1,",
            "full-width correction applies to actual keyboard transaction");
        ok &= require(controller.processPaste("", 0, 0, fullWidth).transaction.text == fullWidth,
            "paste preserves the v2 original-text policy");
        const auto family = settings.codeFont().family();
        settings.setEditorFontFamilyOverride("serif");
        ok &= require(settings.codeFont().family() == "serif" && settings.codeFont().pointSize() == 24,
            "project font preserves preference size");
        settings.setEditorAppearance(18, 1.5);
        ok &= require(settings.codeFont().family() == "serif" && settings.editorBlockSpacing() == 5,
            "project font survives live appearance changes");
        settings.setEditorFontFamilyOverride({});
        ok &= require(settings.codeFont().family() == family, "clearing project font returns to bundled font");
        settings.setThemeModeToken("light");
        settings.setFontSize(14);
        settings.setEditorScrollPastEnd(false);
        settings.setEditorSelectionBeatDisplay(false);
        settings.setPreviewHidePv(true);
        settings.setPreviewCanvasFreeAspect(true);
    }
    {
        WorkbenchSettings restored;
        ok &= require(restored.codeFont().pointSize() == 24 && restored.editorBlockSpacing() == 8,
            "new settings owner restores durable appearance");
        ok &= require(!restored.darkTheme() && restored.themeModeToken() == "light"
            && restored.fontSize() == 14 && !restored.editorScrollPastEnd()
            && !restored.editorSelectionBeatDisplay() && restored.previewHidePv()
            && restored.previewCanvasFreeAspect(), "new owner restores theme and view preferences");
    }
    const auto saved = PreferenceDocument::loadPreferencesObject();
    ok &= require(saved.value("future_section") == initial.value("future_section")
        && saved.value("preview") == initial.value("preview")
        && saved.value("ui").toObject().value("future_editor_setting").toString() == "preserved",
        "unknown settings survive every write");
    qInfo() << "MobileWorkbenchSettingsSpec:" << (ok ? "passed" : "failed");
    return ok ? 0 : 3;
}
