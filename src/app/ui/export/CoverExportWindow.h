#pragma once

#include "app/ui/export/CoverExportSession.h"
#include "app/ui/chrome/PlatformChrome.h"
#include "app/ui/chrome/WindowChrome.h"

#include <QIcon>
#include <QObject>
#include <QPointer>

#include <memory>

class QQmlApplicationEngine;
class QQuickWindow;

namespace miacode::ui {
class WorkbenchSettings;

class CoverExportWindow final : public QObject
{
    Q_OBJECT

public:
    CoverExportWindow(miacode::ExportEngine& exportEngine,
                         miacode::PlaybackControl*& playbackControlSlot,
                         WorkbenchSettings& preferences,
                         const QIcon& icon,
                         QObject* parent = nullptr);
    ~CoverExportWindow() override;

    bool show(QQuickWindow* referenceWindow, int difficultyId);
    void raise();
    void refreshDocument(int difficultyId);
    Q_INVOKABLE void close();

private:
    WorkbenchSettings& preferences_;
    QIcon icon_;
    PlatformChrome platform_;
    WindowChrome windowChrome_;
    // The window's own request queue. It has its own UiRequestHost; on the shell's
    // shared queue, the main window's host would open every pick a second time.
    miacode::UiRequestService requests_;
    CoverExportSession session_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QPointer<QQuickWindow> window_;
    bool closePending_ = false;
    bool refreshPending_ = false;
    int pendingDifficultyId_ = 0;
};
} // namespace miacode::ui
