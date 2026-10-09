#pragma once

#include "AndroidDocumentSession.h"
#include "MobilePreview.h"
#include "app/services/AnalysisService.h"
#include "app/services/EditorSyncController.h"
#include "timeline/TimelineQuickModel.h"
#include "timeline/quick/TimelineQuickStateBridge.h"
#include "common/WaveformCache.h"

namespace miacode::android {

// Platform composition only: all timeline geometry, parsing, wave pyramids,
// analysis and editor navigation gates remain the v2 implementations.
class MobileTimeline final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* stateBridge READ stateBridge CONSTANT)
    Q_PROPERTY(QString currentTabId READ currentTabId NOTIFY tabChanged)
    Q_PROPERTY(bool timelineTabVisible READ tabVisible CONSTANT)
    Q_PROPERTY(bool validationTabVisible READ tabVisible CONSTANT)
    Q_PROPERTY(bool muriTabVisible READ tabVisible CONSTANT)
    Q_PROPERTY(QString timelineTabLabel READ timelineTabLabel NOTIFY labelsChanged)
    Q_PROPERTY(QString validationTabLabel READ validationTabLabel NOTIFY labelsChanged)
    Q_PROPERTY(QString muriTabLabel READ muriTabLabel NOTIFY labelsChanged)
    Q_PROPERTY(QString followCodeLabel READ followCodeLabel NOTIFY labelsChanged)
public:
    MobileTimeline(AndroidDocumentSession& document, MobilePreview& preview,
        EditorSyncController& editor, AnalysisService& analysis, QObject* parent = nullptr);
    QObject* stateBridge() { return &bridge_; }
    QString currentTabId() const { return tab_; }
    bool tabVisible() const { return true; }
    QString timelineTabLabel() const;
    QString validationTabLabel() const;
    QString muriTabLabel() const;
    QString followCodeLabel() const;
    void setFrameRate(PreviewCanvasFrameRateMode mode, double refreshHz);
    Q_INVOKABLE void setCurrentTabId(const QString& tab);
    Q_INVOKABLE void headerNavigate(double second);
    Q_INVOKABLE void wheelNavigate(double second);
    Q_INVOKABLE void centerNavigate(double second);
    Q_INVOKABLE void dragStarted();
    Q_INVOKABLE void dragFinished(double second);
    Q_INVOKABLE void userInteractionStarted();
    Q_INVOKABLE void surfaceReady();
    Q_INVOKABLE void followPreviewToggled(bool enabled);
signals:
    void tabChanged();
    void labelsChanged();
private:
    void rebuild();
    void publishPosition();
    void navigate(double second, bool center);
    AndroidDocumentSession& document_;
    MobilePreview& preview_;
    EditorSyncController& editor_;
    TimelineQuickModel model_;
    TimelineQuickStateBridge bridge_;
    waveform::WaveformCacheService waveforms_;
    QString tab_ = QStringLiteral("timeline");
    QString trackIdentity_;
    quint64 waveformRequest_ = 0;
    bool navigating_ = false;
    QTimer presentationTimer_;
    QElapsedTimer presentationClock_;
    MobileFrameCadence cadence_;
};
}
