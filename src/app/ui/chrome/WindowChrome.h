#pragma once

#include "app/ui/chrome/NativeWindowTheme.h"
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVariantList>

class QWindow;
class QEvent;

// Shared WindowTitleBar chrome. Attach from the owning window controller.
// Windows: native resize frame and system commands; QML owns the caption.
// macOS: full-size content; native title text hidden; QWindow::title kept.
// A non-empty state key enables geometry persistence for that window.
// titleBarLeadingInset: clearance past macOS traffic lights (0 elsewhere).
namespace miacode::ui {

class WindowChrome final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(qreal titleBarLeadingInset READ titleBarLeadingInset NOTIFY titleBarLeadingInsetChanged FINAL)
    Q_PROPERTY(qreal titleBarHeight READ titleBarHeight NOTIFY titleBarHeightChanged FINAL)
    Q_PROPERTY(bool nativeMaterialAvailable READ nativeMaterialAvailable NOTIFY nativeMaterialAvailableChanged FINAL)
    Q_PROPERTY(QVariantList materialRegions READ materialRegions WRITE setMaterialRegions NOTIFY materialRegionsChanged FINAL)

public:
    explicit WindowChrome(QObject* parent = nullptr);
    ~WindowChrome() override;

    void attach(QWindow* window, const QString& stateKey = QStringLiteral("main_window"));
    Q_INVOKABLE void showRestored();
#ifdef Q_OS_IOS
    void applyIos(QWindow* window);
    Q_INVOKABLE void refreshTitleBarMetrics();
#endif
    Q_INVOKABLE void minimize();
    Q_INVOKABLE void toggleMaximized();
    Q_INVOKABLE void handleTitleBarDoubleClick();
    Q_INVOKABLE void saveWindowState();
    qreal titleBarLeadingInset() const { return titleBarLeadingInset_; }
    qreal titleBarHeight() const { return titleBarHeight_; }
    bool nativeMaterialAvailable() const { return nativeMaterialAvailable_; }
    QVariantList materialRegions() const { return materialRegions_; }
    void setMaterialRegions(const QVariantList& regions);
    bool blurMaterialsEnabled() const { return blurMaterialsEnabled_; }
    void setBlurMaterialsEnabled(bool enabled);
    void refreshNativeTheme();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void titleBarLeadingInsetChanged();
    void titleBarHeightChanged();
    void nativeMaterialAvailableChanged();
    void materialRegionsChanged();

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void refreshNativeMaterial();
#ifdef Q_OS_MACOS
    void refreshMaterialAfterPresentation();
#endif
    bool extendDwmFrame() const;
    void observeMacOsWindow(QWindow* window);
    void stopObservingMacOsWindow();
    void updateMacOsTitleBarMetrics(QWindow* window);
    void refreshMacOsMaterial(QWindow* window);
    void observeMacOsFullScreen(QWindow* window);
    void stopObservingMacOsFullScreen();
    void releaseMacOsMaterial();
    void setTitleBarLeadingInset(qreal inset);
    void setTitleBarHeight(qreal height);
    void setNativeMaterialAvailable(bool available);
    void restoreWindowState();
    void captureWindowState();

    QPointer<QWindow> window_;
    QRect normalGeometry_;
    QString screenName_;
    bool maximized_ = false;
    bool restoringWindowState_ = false;
    QString stateKey_;
    QTimer stateCaptureTimer_;
    QTimer materialUpdateTimer_;
#ifdef Q_OS_MACOS
    bool materialPresentationPending_ = false;
#endif
    quintptr nativeHandle_ = 0;
#ifdef Q_OS_WIN
    NativeWindowTheme::AppliedState dwmState_;
#endif
    bool nativeMaterialAvailable_ = false;
    QVariantList materialRegions_;
    bool blurMaterialsEnabled_ = true;
    qreal titleBarLeadingInset_ = 0;
    qreal titleBarHeight_ = 0;
    qreal windowedTitleBarLeadingInset_ = 0;
    qreal windowedTitleBarHeight_ = 0;
    void* macViewWindowObserver_ = nullptr;
    void* macWillEnterFullScreenObserver_ = nullptr;
    void* macMaterialView_ = nullptr;
    void* macDidEnterFullScreenObserver_ = nullptr;
    void* macWillExitFullScreenObserver_ = nullptr;
    void* macDidExitFullScreenObserver_ = nullptr;
};
} // namespace miacode::ui
