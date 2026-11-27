#include "app/ui/chrome/NativeWindowTheme.h"

#include "app/ui/theme/UiTheme.h"

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

bool applyBackdropToNativeHandle(HWND hwnd, bool backdropEnabled, AppliedState* state)
{
    const int backdropType = backdropEnabled ? kDwmsbtTransientWindow : kDwmsbtNone;
    if (state != nullptr && state->backdropType == backdropType) {
        return backdropEnabled;
    }
    const bool backdropApplied = setDwmWindowAttribute(
        hwnd, kDwmwaSystemBackdropType, &backdropType, sizeof(backdropType));
    if (backdropApplied && state != nullptr) {
        state->backdropType = backdropType;
    }

    return backdropEnabled && backdropApplied;
}
#endif  // Q_OS_WIN

}  // namespace

void applyAppearanceToWindow(QWindow* window, AppliedState* state)
{
#ifdef Q_OS_WIN
    if (window == nullptr) {
        return;
    }
    applyAppearanceToNativeHandle(reinterpret_cast<HWND>(window->winId()), state);
#else
    Q_UNUSED(window);
    Q_UNUSED(state);
#endif
}

bool applyToWindow(QWindow* window, bool backdropEnabled, AppliedState* state)
{
#ifdef Q_OS_WIN
    if (window == nullptr) {
        return false;
    }
    applyAppearanceToWindow(window, state);
    return applyBackdropToNativeHandle(reinterpret_cast<HWND>(window->winId()), backdropEnabled, state);
#else
    Q_UNUSED(window);
    Q_UNUSED(backdropEnabled);
    Q_UNUSED(state);
    return false;
#endif
}

}  // namespace NativeWindowTheme
