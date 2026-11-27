#pragma once

#include <optional>

class QWindow;

// Windows non-client DWM theming helper; non-Windows targets keep no-ops.
namespace NativeWindowTheme {

// Successful DWM settings, owned by the window's chrome lifecycle.
struct AppliedState {
    std::optional<bool> darkMode;
    std::optional<int> backdropType;
    bool frameExtended = false;
};

// Updates appearance without rebuilding the window's backdrop.
void applyAppearanceToWindow(QWindow* window, AppliedState* state = nullptr);

// Sets appearance and native Acrylic backdrop on Windows. Returns whether accepted.
bool applyToWindow(QWindow* window, bool backdropEnabled = true, AppliedState* state = nullptr);

}  // namespace NativeWindowTheme
