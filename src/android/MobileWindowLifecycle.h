#pragma once

#include <QEvent>
#include <QGuiApplication>
#include <QJniObject>
#include <QJniEnvironment>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QTimer>
#include <functional>
#include <utility>

namespace miacode::android {
inline const QLoggingCategory& mobileWindowLifecycleLog()
{
    static const QLoggingCategory category("miacode.android.window", QtWarningMsg);
    return category;
}
// An Android surface-change expose can arrive after onPause but before Qt's
// queued application-state notification. Only the onscreen root is
// filtered: render-control capture windows retain their normal lifecycle.
class MobileWindowLifecycle final : public QObject {
public:
    MobileWindowLifecycle(QQuickWindow& window, std::function<bool()> exporting)
        : QObject(&window), window_(window), exporting_(std::move(exporting))
    {
        window_.installEventFilter(this);
        connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive && suppressed_) {
                    suppressed_ = false;
                    qCDebug(mobileWindowLifecycleLog) << "resuming onscreen redraw";
                    QTimer::singleShot(0, &window_, &QQuickWindow::requestUpdate);
                }
            });
    }
protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        const bool exposedRedraw = event->type() == QEvent::Expose && window_.isExposed();
        if (object != &window_ || (!exposedRedraw && event->type() != QEvent::UpdateRequest)
            || (!exporting_() && !suppressed_) || !QNativeInterface::QAndroidApplication::isActivityContext())
            return false;
        const auto activity = QNativeInterface::QAndroidApplication::context();
        const bool resumed = activity.callMethod<jboolean>("isUiResumed");
        QJniEnvironment environment;
        if (environment.checkAndClearExceptions()) return false;
        if (resumed) {
            suppressed_ = false;
            return false;
        }
        if (!suppressed_) {
            qCDebug(mobileWindowLifecycleLog) << "suppressing onscreen redraw" << event->type();
            suppressed_ = true;
        }
        return true;
    }
private:
    QQuickWindow& window_;
    std::function<bool()> exporting_;
    bool suppressed_ = false;
};
}
