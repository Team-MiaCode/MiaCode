#include "app/ui/chrome/NativeWindowThemeMac.h"

#include <QGuiApplication>

#import <AppKit/AppKit.h>

namespace NativeWindowThemeMac {

void applyToNativeView(
    void* nativeViewHandle,
    NativeWindowThemePolicy::Appearance appearance)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        return;
    }
    NSView* view = (__bridge NSView*)nativeViewHandle;
    NSWindow* window = (view != nil) ? view.window : nil;
    if (window == nil) {
        return;
    }

    switch (appearance) {
    case NativeWindowThemePolicy::Appearance::Light:
        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
        break;
    case NativeWindowThemePolicy::Appearance::Dark:
        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        break;
    case NativeWindowThemePolicy::Appearance::System:
    default:
        window.appearance = nil;
        break;
    }
}

}  // namespace NativeWindowThemeMac
