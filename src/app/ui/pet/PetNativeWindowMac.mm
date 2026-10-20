#include <QByteArray>
#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QVariant>
#include <QWindow>

#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

namespace miacode::ui {
namespace {

char kPetWindowKey;

bool isPetWindow(id object)
{
    return object != nil && objc_getAssociatedObject(object, &kPetWindowKey) != nil;
}

void setPreventsActivation(NSWindow* window)
{
    // NSPanel 在创建后改变 styleMask 时，需要同步 WindowServer 的激活状态。
    // 使用标量 BOOL 的函数签名；接口可用性由当前系统判断。
    const SEL selector = sel_registerName("_setPreventsActivation:");
    if ([window respondsToSelector:selector]) {
        reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(window, selector, YES);
    } else {
        static bool reported = false;
        if (!reported) {
            reported = true;
            qWarning("Desktop pet: native activation compatibility selector unavailable");
        }
    }
}

void specializePetPanel(NSWindow* window)
{
    if (isPetWindow(window))
        return;

    // 继承实际原生类，为桌宠实例追加行为；Qt 的原生类保持原有方法。
    const Class base = object_getClass(window);
    const QByteArray name = QByteArray("MiaCodePetPanel_") + class_getName(base);
    Class petClass = objc_getClass(name.constData());
    if (petClass == Nil) {
        petClass = objc_allocateClassPair(base, name.constData(), 0);
        const Method styleMethod = class_getInstanceMethod(base, @selector(setStyleMask:));
        class_addMethod(petClass, @selector(setStyleMask:),
            imp_implementationWithBlock(^(id self, NSWindowStyleMask mask) {
                objc_super parent = {self, base};
                reinterpret_cast<void (*)(objc_super*, SEL, NSWindowStyleMask)>(objc_msgSendSuper)(
                    &parent, @selector(setStyleMask:), mask | NSWindowStyleMaskNonactivatingPanel);
                setPreventsActivation(static_cast<NSWindow*>(self));
            }), method_getTypeEncoding(styleMethod));
        const Method mainMethod = class_getInstanceMethod(base, @selector(canBecomeMainWindow));
        class_addMethod(petClass, @selector(canBecomeMainWindow),
            imp_implementationWithBlock(^BOOL(id) { return NO; }),
            method_getTypeEncoding(mainMethod));
        objc_registerClassPair(petClass);
    }
    object_setClass(window, petClass);
    objc_setAssociatedObject(window, &kPetWindowKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

void applyPetNativeWindow(QWindow* window)
{
    if (window == nullptr)
        return;
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
    NSWindow* nativeWindow = view.window;
    if (![nativeWindow isKindOfClass:[NSPanel class]])
        return;

    specializePetPanel(nativeWindow);
    nativeWindow.styleMask |= NSWindowStyleMaskNonactivatingPanel;
    nativeWindow.hidesOnDeactivate = NO;
    static_cast<NSPanel*>(nativeWindow).becomesKeyOnlyIfNeeded = YES;
}

class PetNativeWindowFilter final : public QObject
{
public:
    explicit PetNativeWindowFilter(QWindow* window)
        : QObject(window), window_(window)
    {
        window->installEventFilter(this);
        applyPetNativeWindow(window);
        monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:
                        (NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown | NSEventMaskOtherMouseDown)
                    handler:^NSEvent*(NSEvent* event) {
            if (isPetWindow(event.window))
                [NSApp preventWindowOrdering];
            return event;
        }];
    }

    ~PetNativeWindowFilter() override
    {
        if (monitor_ != nil)
            [NSEvent removeMonitor:monitor_];
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == window_.data()
            && (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange)) {
            if (!pending_) {
                pending_ = true;
                QTimer::singleShot(0, this, [this]() {
                    pending_ = false;
                    if (window_)
                        applyPetNativeWindow(window_.data());
                });
            }
        }
        return false;
    }

private:
    QPointer<QWindow> window_;
    id monitor_ = nil;
    bool pending_ = false;
};

} // namespace

void attachPetNativeWindow(QWindow* window)
{
    if (window == nullptr || QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;
    if (window->property("_miacodePetNativeAttached").toBool()) {
        applyPetNativeWindow(window);
        return;
    }
    window->setProperty("_miacodePetNativeAttached", true);
    new PetNativeWindowFilter(window);
}

} // namespace miacode::ui
