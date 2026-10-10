#include "app/ui/chrome/WindowChrome.h"
#include "app/ui/chrome/NativeWindowTheme.h"
#include "app/services/PreferenceDocument.h"
#include "common/DebugLog.h"

#include <QtGlobal>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonObject>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QScreen>
#include <QScopedValueRollback>
#include <QQuickWindow>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

namespace miacode::ui {

WindowChrome::WindowChrome(QObject* parent)
    : QObject(parent)
{
    // Capture after the geometry and window-state events of a transition settle.
    stateCaptureTimer_.setSingleShot(true);
    stateCaptureTimer_.setInterval(0);
    connect(&stateCaptureTimer_, &QTimer::timeout, this, &WindowChrome::captureWindowState);
    materialUpdateTimer_.setSingleShot(true);
    materialUpdateTimer_.setInterval(0);
    connect(&materialUpdateTimer_, &QTimer::timeout, this, &WindowChrome::refreshNativeMaterial);
}

WindowChrome::~WindowChrome()
{
    stopObservingMacOsWindow();
    stopObservingMacOsFullScreen();
    releaseMacOsMaterial();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

void WindowChrome::setTitleBarLeadingInset(qreal inset)
{
    if (qFuzzyCompare(titleBarLeadingInset_, inset)) {
        return;
    }
    titleBarLeadingInset_ = inset;
    emit titleBarLeadingInsetChanged();
}

void WindowChrome::setTitleBarHeight(qreal height)
{
    if (qFuzzyCompare(titleBarHeight_, height)) {
        return;
    }
    titleBarHeight_ = height;
    emit titleBarHeightChanged();
}

void WindowChrome::setNativeMaterialAvailable(bool available)
{
    if (nativeMaterialAvailable_ == available) {
        return;
    }
    nativeMaterialAvailable_ = available;
    emit nativeMaterialAvailableChanged();
}

void WindowChrome::setMaterialRegions(const QVariantList& regions)
{
    if (materialRegions_ == regions) {
        return;
    }
#ifdef Q_OS_WIN
    const bool materialVisibilityChanged = materialRegions_.isEmpty() != regions.isEmpty();
#endif
    materialRegions_ = regions;
    emit materialRegionsChanged();
#ifdef Q_OS_WIN
    if (materialVisibilityChanged) {
        materialUpdateTimer_.start();
    }
#elif defined(Q_OS_MACOS)
    materialUpdateTimer_.start();
#endif
}

void WindowChrome::attach(QWindow* window, const QString& stateKey)
{
    if (window == nullptr) {
        return;
    }

    window_ = window;
    stateKey_ = stateKey;

#ifdef Q_OS_WIN
    nativeHandle_ = window->winId();
    window->installEventFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(this);
    SetWindowPos(reinterpret_cast<HWND>(nativeHandle_), nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    materialUpdateTimer_.start();
    setTitleBarLeadingInset(0);
#elif defined(Q_OS_MACOS)
    window->winId();
    window->installEventFilter(this);
    observeMacOsWindow(window);
    updateMacOsTitleBarMetrics(window);
#elif defined(Q_OS_IOS)
    applyIos(window);
    return;
#else
    Q_UNUSED(window);
    setTitleBarLeadingInset(0);
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (auto* quickWindow = qobject_cast<QQuickWindow*>(window)) {
#ifdef Q_OS_WIN
        QObject::connect(quickWindow, &QQuickWindow::sceneGraphInitialized, this,
            [this] { materialUpdateTimer_.start(); }, Qt::QueuedConnection);
#else
        // AppKit attaches its effect view after Qt presents its content layer.
        QObject::connect(quickWindow, &QQuickWindow::sceneGraphInitialized, this,
            &WindowChrome::refreshMaterialAfterPresentation, Qt::QueuedConnection);
#endif
        QObject::connect(quickWindow, &QQuickWindow::sceneGraphInvalidated, this,
            [this] {
                materialUpdateTimer_.stop();
#ifdef Q_OS_MACOS
                releaseMacOsMaterial();
#else
                dwmState_ = {};
#endif
                setNativeMaterialAvailable(false);
            }, Qt::QueuedConnection);
        if (quickWindow->isSceneGraphInitialized()) {
#ifdef Q_OS_WIN
            materialUpdateTimer_.start();
#else
            refreshMaterialAfterPresentation();
#endif
        }
    }
#endif

    // Restore client coordinates after the platform chrome establishes its
    // frame margins, using the same coordinate space as captureWindowState.
    restoreWindowState();
    const auto scheduleCapture = [this]() { stateCaptureTimer_.start(); };
    connect(window, &QWindow::xChanged, this, scheduleCapture);
    connect(window, &QWindow::yChanged, this, scheduleCapture);
    connect(window, &QWindow::widthChanged, this, scheduleCapture);
    connect(window, &QWindow::heightChanged, this, scheduleCapture);
    connect(window, &QWindow::windowStateChanged, this, scheduleCapture);
    connect(window, &QWindow::visibilityChanged, this, scheduleCapture);
    connect(window, &QWindow::screenChanged, this, scheduleCapture);
}

void WindowChrome::showRestored()
{
    if (window_.isNull()) {
        return;
    }
    if (maximized_ && window_->screen() != nullptr) {
        window_->setWindowStates(Qt::WindowNoState);
        window_->setGeometry(window_->screen()->availableGeometry());
    }
    maximized_ = false;
    window_->showNormal();
}

void WindowChrome::minimize()
{
    if (window_.isNull()) {
        return;
    }
    captureWindowState();
    // Preserve the restore state when minimizing from maximized or fullscreen.
    window_->setWindowStates(window_->windowStates() | Qt::WindowMinimized);
}

void WindowChrome::toggleMaximized()
{
    if (window_.isNull()) {
        return;
    }
    if (window_->windowStates().testFlag(Qt::WindowMaximized)) {
        window_->showNormal();
    } else {
        captureWindowState();
        window_->showMaximized();
    }
}

void WindowChrome::setBlurMaterialsEnabled(bool enabled)
{
    if (blurMaterialsEnabled_ == enabled) {
        return;
    }
    blurMaterialsEnabled_ = enabled;
    if (window_.isNull()) {
        return;
    }
    // Settings listeners run before the QML material-region bindings settle.
    materialUpdateTimer_.start();
}

void WindowChrome::refreshNativeTheme()
{
#ifdef Q_OS_WIN
    if (nativeHandle_ == 0) {
        return;
    }
    NativeWindowTheme::applyAppearanceToWindow(window_.data(), &dwmState_);
#else
    NativeWindowTheme::applyAppearanceToWindow(window_.data());
#endif
}

void WindowChrome::refreshNativeMaterial()
{
    if (window_.isNull()) {
        return;
    }
#ifdef Q_OS_WIN
    if (nativeHandle_ == 0 || !window_->isExposed()) {
        return;
    }
    if (!dwmState_.frameExtended) {
        dwmState_.frameExtended = extendDwmFrame();
    }
    const bool backdropApplied = NativeWindowTheme::applyToWindow(
        window_.data(), blurMaterialsEnabled_ && !materialRegions_.isEmpty(),
        NativeWindowTheme::BackdropMaterial::Acrylic, &dwmState_);
    setNativeMaterialAvailable(dwmState_.frameExtended && backdropApplied);
    if (auto* quickWindow = qobject_cast<QQuickWindow*>(window_.data())) {
        // Present the transparent client content after changing composition.
        quickWindow->update();
    }
#elif defined(Q_OS_MACOS)
    refreshMacOsMaterial(window_.data());
    updateMacOsTitleBarMetrics(window_.data());
#endif
}

#ifdef Q_OS_MACOS
void WindowChrome::refreshMaterialAfterPresentation()
{
    auto* quickWindow = qobject_cast<QQuickWindow*>(window_.data());
    if (quickWindow == nullptr) {
        return;
    }
    if (materialPresentationPending_) {
        return;
    }
    materialPresentationPending_ = true;
    QObject::connect(
        quickWindow, &QQuickWindow::frameSwapped, this,
        [this] {
            materialPresentationPending_ = false;
            materialUpdateTimer_.start();
        }, Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
    quickWindow->update();
}
#endif

bool WindowChrome::eventFilter(QObject* watched, QEvent* event)
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (watched == window_.data()) {
        if (event->type() == QEvent::Show) {
            // Apply after Qt has completed the native show operation.
#ifdef Q_OS_WIN
            dwmState_ = {};
            materialUpdateTimer_.start();
#else
            refreshMaterialAfterPresentation();
#endif
        } else if (
#ifdef Q_OS_MACOS
                event->type() == QEvent::Expose ||
#endif
                event->type() == QEvent::WindowStateChange
                || event->type() == QEvent::ScreenChangeInternal) {
#ifdef Q_OS_WIN
            // Frame and backdrop settings belong to the resulting native state.
            dwmState_ = {};
            materialUpdateTimer_.start();
#else
            if (window_->isExposed()) {
                refreshMaterialAfterPresentation();
            }
#endif
#ifdef Q_OS_WIN
        } else if (event->type() == QEvent::Expose
            && window_->isExposed() && !dwmState_.frameExtended) {
            // A show event can precede exposure; apply at the first exposure.
            materialUpdateTimer_.start();
#endif
        } else if (event->type() == QEvent::PlatformSurface) {
            const auto* surfaceEvent = static_cast<QPlatformSurfaceEvent*>(event);
            if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
#ifdef Q_OS_WIN
                QScopedValueRollback<bool> restoring(restoringWindowState_, true);
                // Creation uses the system caption margins. Establish our
                // client frame before applying the saved Qt client geometry.
                nativeHandle_ = window_->winId();
                SetWindowPos(reinterpret_cast<HWND>(nativeHandle_), nullptr, 0, 0, 0, 0,
                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                if (normalGeometry_.isValid()
                    && window_->windowStates() == Qt::WindowNoState) {
                    window_->setGeometry(normalGeometry_);
                }
#endif
#ifdef Q_OS_MACOS
                observeMacOsWindow(window_.data());
#endif
                materialUpdateTimer_.start();
            } else {
                materialUpdateTimer_.stop();
#ifdef Q_OS_MACOS
                stopObservingMacOsWindow();
                stopObservingMacOsFullScreen();
                releaseMacOsMaterial();
#elif defined(Q_OS_WIN)
                dwmState_ = {};
#endif
                nativeHandle_ = 0;
                setNativeMaterialAvailable(false);
            }
        }
    }
#endif
    return QObject::eventFilter(watched, event);
}

void WindowChrome::restoreWindowState()
{
    if (stateKey_.isEmpty()) {
        return;
    }
    const QJsonObject saved = PreferenceDocument::loadPreferencesObject()
        .value(QStringLiteral("ui")).toObject()
        .value(stateKey_).toObject();
    const QJsonObject geometry = saved.value(QStringLiteral("normal_geometry")).toObject();
    normalGeometry_ = QRect(geometry.value(QStringLiteral("x")).toInt(),
                            geometry.value(QStringLiteral("y")).toInt(),
                            geometry.value(QStringLiteral("width")).toInt(),
                            geometry.value(QStringLiteral("height")).toInt());
    if (!normalGeometry_.isValid()) {
        normalGeometry_ = window_->geometry();
        screenName_ = window_->screen() ? window_->screen()->name() : QString();
        return;
    }

    // Wayland's compositor owns top-level placement; restore only size and state.
    const bool canRestorePosition = !QGuiApplication::platformName().startsWith(QStringLiteral("wayland"));
    QScreen* screen = nullptr;
    if (canRestorePosition) {
        const QString savedScreenName = saved.value(QStringLiteral("screen_name")).toString();
        for (QScreen* candidate : QGuiApplication::screens()) {
            if (candidate->name() == savedScreenName) {
                screen = candidate;
                break;
            }
        }
        if (screen == nullptr) {
            screen = QGuiApplication::screenAt(normalGeometry_.center());
        }
    }
    if (screen == nullptr) {
        screen = window_->screen();
    }
    if (screen != nullptr) {
        const QRect available = screen->availableGeometry();
        normalGeometry_.setSize(QSize(
            qBound(qMin(window_->minimumWidth(), available.width()), normalGeometry_.width(), available.width()),
            qBound(qMin(window_->minimumHeight(), available.height()), normalGeometry_.height(), available.height())));
        if (canRestorePosition) {
            normalGeometry_.moveLeft(qBound(available.left(), normalGeometry_.left(),
                                           available.right() - normalGeometry_.width() + 1));
            normalGeometry_.moveTop(qBound(available.top(), normalGeometry_.top(),
                                          available.bottom() - normalGeometry_.height() + 1));
            window_->setScreen(screen);
        }
        screenName_ = screen->name();
        if (saved.value(QStringLiteral("maximized")).toBool()) {
            normalGeometry_ = available;
        }
    }
    if (canRestorePosition) {
        window_->setGeometry(normalGeometry_);
    } else {
        window_->resize(normalGeometry_.size());
    }
    maximized_ = false;
}

void WindowChrome::captureWindowState()
{
    if (window_.isNull() || !window_->isVisible() || restoringWindowState_) {
        return;
    }
    const Qt::WindowStates states = window_->windowStates();
    if (states.testFlag(Qt::WindowMinimized) || states.testFlag(Qt::WindowFullScreen)) {
        return;
    }
    maximized_ = states.testFlag(Qt::WindowMaximized);
    if (!maximized_) {
        normalGeometry_ = window_->geometry();
    }
    if (window_->screen() != nullptr) {
        screenName_ = window_->screen()->name();
    }
}

void WindowChrome::saveWindowState()
{
#ifdef Q_OS_IOS
    return;
#endif
    if (window_.isNull() || stateKey_.isEmpty()) {
        return;
    }
    captureWindowState();
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject ui = root.value(QStringLiteral("ui")).toObject();
    ui.insert(stateKey_, QJsonObject{
        {QStringLiteral("normal_geometry"), QJsonObject{
             {QStringLiteral("x"), normalGeometry_.x()},
             {QStringLiteral("y"), normalGeometry_.y()},
             {QStringLiteral("width"), normalGeometry_.width()},
             {QStringLiteral("height"), normalGeometry_.height()}}},
        {QStringLiteral("screen_name"), screenName_},
        {QStringLiteral("maximized"), maximized_}});
    root.insert(QStringLiteral("ui"), ui);
    if (!PreferenceDocument::savePreferencesObject(root)) {
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
                                     QStringLiteral("window"), QStringLiteral("action=state_save_failed"));
    }
}

#ifdef Q_OS_IOS
void WindowChrome::refreshTitleBarMetrics()
{
    if (window_)
        applyIos(window_.data());
}
#endif

bool WindowChrome::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" || nativeHandle_ == 0) {
        return false;
    }

    const auto* nativeMessage = static_cast<MSG*>(message);
    const auto handle = reinterpret_cast<HWND>(nativeHandle_);
    if (nativeMessage->hwnd != handle) {
        return false;
    }

    if (nativeMessage->message == WM_NCACTIVATE) {
        // Preserve native activation while the QML caption owns its pixels.
        *result = DefWindowProcW(handle, WM_NCACTIVATE, nativeMessage->wParam, -1);
        return true;
    }

    if (nativeMessage->message == WM_NCCALCSIZE) {
        if (IsZoomed(handle) && !window_->windowStates().testFlag(Qt::WindowFullScreen)) {
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            const HMONITOR monitor = MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);
            if (GetMonitorInfoW(monitor, &monitorInfo)) {
                // QML owns the caption, so its client origin is the work-area
                // origin rather than the native caption's client origin.
                auto* clientRect = nativeMessage->wParam == TRUE
                    ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(nativeMessage->lParam)->rgrc[0]
                    : reinterpret_cast<RECT*>(nativeMessage->lParam);
                *clientRect = monitorInfo.rcWork;
            }
        }

        *result = 0;
        return true;
    }

    if (nativeMessage->message == WM_NCHITTEST) {
        if (IsZoomed(handle) || window_->windowStates().testFlag(Qt::WindowFullScreen)) {
            *result = HTCLIENT;
            return true;
        }

        RECT windowRect{};
        GetWindowRect(handle, &windowRect);

        const UINT dpi = GetDpiForWindow(handle);
        const int horizontalBorder = GetSystemMetricsForDpi(SM_CXFRAME, dpi)
            + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const int verticalBorder = GetSystemMetricsForDpi(SM_CYFRAME, dpi)
            + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const POINT cursorPosition{
            GET_X_LPARAM(nativeMessage->lParam),
            GET_Y_LPARAM(nativeMessage->lParam)};

        const bool left = cursorPosition.x < windowRect.left + horizontalBorder;
        const bool right = cursorPosition.x >= windowRect.right - horizontalBorder;
        const bool top = cursorPosition.y < windowRect.top + verticalBorder;
        const bool bottom = cursorPosition.y >= windowRect.bottom - verticalBorder;

        if (top && left)
            *result = HTTOPLEFT;
        else if (top && right)
            *result = HTTOPRIGHT;
        else if (bottom && left)
            *result = HTBOTTOMLEFT;
        else if (bottom && right)
            *result = HTBOTTOMRIGHT;
        else if (left)
            *result = HTLEFT;
        else if (right)
            *result = HTRIGHT;
        else if (top)
            *result = HTTOP;
        else if (bottom)
            *result = HTBOTTOM;
        else
            *result = HTCLIENT;

        return true;
    }

    if (nativeMessage->message == WM_DWMCOMPOSITIONCHANGED) {
        dwmState_ = {};
        materialUpdateTimer_.start();
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif

    return false;
}

bool WindowChrome::extendDwmFrame() const
{
#ifdef Q_OS_WIN
    if (nativeHandle_ == 0) {
        return false;
    }

    const auto handle = reinterpret_cast<HWND>(nativeHandle_);
    // QML owns the caption; retain the native frame's animation styles.
    const MARGINS margins{1, 1, 0, 1};
    const HRESULT frameResult = DwmExtendFrameIntoClientArea(handle, &margins);
    return SUCCEEDED(frameResult);
#else
    return false;
#endif
}

#ifndef Q_OS_MACOS
void WindowChrome::handleTitleBarDoubleClick()
{
    toggleMaximized();
}

void WindowChrome::refreshMacOsMaterial(QWindow* window)
{
    Q_UNUSED(window);
}

void WindowChrome::observeMacOsWindow(QWindow* window)
{
    Q_UNUSED(window);
}

void WindowChrome::stopObservingMacOsWindow()
{
}

void WindowChrome::updateMacOsTitleBarMetrics(QWindow* window)
{
    Q_UNUSED(window);
}

void WindowChrome::observeMacOsFullScreen(QWindow* window)
{
    Q_UNUSED(window);
}

void WindowChrome::stopObservingMacOsFullScreen()
{
}

void WindowChrome::releaseMacOsMaterial()
{
}
#endif

} // namespace miacode::ui
