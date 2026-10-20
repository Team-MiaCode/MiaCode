#pragma once

#include "app/runtime/Session.h"

namespace miacode::runtime {

class EditorHost {
public:
    EditorHost(Session& session, RuntimeContext::Ui& ui, RuntimeContext::State& state);

    void loadPortableState();
    void resetPortablePreviewSettingsToDefaults();
    void applyPortablePreviewSettings(const QJsonObject& preview);
    void savePortableState() const;
    void persistEditorTextFontPreference() const;
    void applyEditorTextFontSize(int pointSize, bool persistPreference);
    void applyEditorLineSpacingFactor(double factor, bool persistPreference);
    void applyEditorHalfWidthInputEnabled(bool enabled, bool persistPreference);
    void applyEditorOverwriteModeEnabled(bool enabled, bool persistPreference);
    void applyEditorAutoCompletionEnabled(bool enabled, bool persistPreference);
    void applyEditorImeInputDisabled(bool disabled, bool persistPreference);
    void applyEditorCompletionCandidates(const QStringList& durations,
                                        const QStringList& divisions, bool persistPreference);
    QString resolveProjectRenderStateFilePath() const;
    void loadProjectRenderState();
    void saveProjectRenderState() const;
    void removeProjectRenderState() const;

private:
    Session& session_;
    RuntimeContext::Ui& ui_;
    RuntimeContext::State& state_;
    // The preview appearance settings are owned by the application assembly,
    // not by the window; this is the same single copy Session binds to.
    miacode::PreviewAppearanceState::Values& previewAppearanceValues_;
};

}  // namespace miacode::runtime
