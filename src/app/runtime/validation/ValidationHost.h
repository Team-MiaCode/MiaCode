#pragma once

#include "app/runtime/Session.h"

#include "app/services/PlaybackValidationPort.h"

namespace miacode::runtime {

class ValidationHost final : public miacode::PlaybackValidationPort {
public:
    ValidationHost(Session& session, RuntimeContext::Ui& ui, RuntimeContext::State& state);

    void loadProjectValidationPreferences();
    void saveProjectValidationPreferences(const QString& chartFilePath = QString()) const;
    void applyIgnoreMuriIssuePrompts(bool enabled, bool persistPreference);
    const MuriAnalysisReport& alignedMuriAnalysisReportForPreview() const;
    void applyAlignedMuriAnalysisReportToViews() override;
    void clearValidationDecorations() override;
    void addValidationDecoration(int line, int col, const QString& message, int endCol = -1);
    void clearValidationCache() override;
    void applyDeferredAnalysisUiUpdates() override;
    void notifyDocumentValidationChanged() override;
    void refreshValidationPanelForActiveField();
    void applyMuriRenderOptions();
    void setMuriRenderMode(RenderMode mode, bool persistState = true) override;
    void setMuriHandRadiusPx(int radiusPx) override;
    void setStaticTapOnSlideThresholdMs(int thresholdMs) override;
    void onToggleJudgeMarkers(bool checked);
    void onToggleTouchTrail(bool checked);
    bool runValidateSimaiSilently();
    Session::DocumentValidationSnapshot documentValidationSnapshot() const;
    Session::QmlAnalysisSnapshot qmlAnalysisSnapshot() const;

private:
    void applyMuriParameterChange();

    Session& session_;
    RuntimeContext::Ui& ui_;
    RuntimeContext::State& state_;
};

}  // namespace miacode::runtime
