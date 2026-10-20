#include "app/ui/chrome/NativeWindowTheme.h"

#include "app/ui/theme/UiTheme.h"

#ifdef Q_OS_MACOS
#include "app/ui/chrome/NativeWindowThemeMac.h"
#endif

#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace NativeWindowTheme {
namespace {

#ifdef Q_OS_WIN
constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
constexpr DWORD kDwmwaBorderColor = 34;
constexpr COLORREF kDwmColorNone = 0xFFFFFFFE;
constexpr DWORD kDwmwaSystemBackdropType = 38;
constexpr int kDwmsbtNone = 1;
constexpr int kDwmsbtMainWindow = 2;
constexpr int kDwmsbtTransientWindow = 3;

bool setDwmWindowAttribute(HWND hwnd, DWORD attribute, const void* value, DWORD size)
{
    return SUCCEEDED(DwmSetWindowAttribute(hwnd, attribute, value, size));
}

void applyAppearanceToNativeHandle(HWND hwnd, AppliedState* state)
{
    if (hwnd == nullptr) {
        return;
    }

    const BOOL darkMode = UiTheme::isDarkTheme() ? TRUE : FALSE;
    if (state != nullptr && state->darkMode == (darkMode != FALSE)) {
        return;
    }
    setDwmWindowAttribute(hwnd, kDwmwaBorderColor, &kDwmColorNone, sizeof(kDwmColorNone));
    if (setDwmWindowAttribute(hwnd, kDwmwaUseImmersiveDarkMode, &darkMode, sizeof(darkMode))
            && state != nullptr) {
        state->darkMode = darkMode != FALSE;
    }
}

bool applyBackdropToNativeHandle(HWND hwnd, bool backdropEnabled, BackdropMaterial material,
                                AppliedState* state)
{
    const int backdropType = backdropEnabled
        ? (material == BackdropMaterial::Acrylic ? kDwmsbtTransientWindow : kDwmsbtMainWindow)
        : kDwmsbtNone;
    if (state != nullptr && state->backdropType == backdropType) {
        return backdropEnabled;
    }
    const bool backdropApplied = setDwmWindowAttribute(hwnd, kDwmwaSystemBackdropType, &backdropType, sizeof(backdropType));
    if (backdropApplied && state != nullptr) {
        state->backdropType = backdropType;
    }

    return backdropEnabled && backdropApplied;
}

#endif  // Q_OS_WIN

}  // namespace

void applyAppearanceToWindow(QWindow* window, AppliedState* state)
{
#ifndef Q_OS_WIN
    Q_UNUSED(state);
#endif
    if (window == nullptr) {
        return;
    }
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    applyAppearanceToNativeHandle(hwnd, state);
#elif defined(Q_OS_MACOS)
    NativeWindowThemeMac::applyToNativeView(
        reinterpret_cast<void*>(window->winId()),
        UiTheme::isDarkTheme()
            ? NativeWindowThemePolicy::Appearance::Dark
            : NativeWindowThemePolicy::Appearance::Light);
#else
    Q_UNUSED(window);
#endif
}

bool applyToWindow(QWindow* window, bool backdropEnabled, BackdropMaterial material,
                   AppliedState* state)
{
    if (window == nullptr) {
        return false;
    }
    applyAppearanceToWindow(window, state);
#ifdef Q_OS_WIN
    return applyBackdropToNativeHandle(reinterpret_cast<HWND>(window->winId()), backdropEnabled,
                                      material, state);
#else
    Q_UNUSED(backdropEnabled);
    Q_UNUSED(material);
#endif
    return false;
}

}  // namespace NativeWindowTheme
