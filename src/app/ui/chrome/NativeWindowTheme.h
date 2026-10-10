#pragma once

#include <optional>

class QWindow;

// Native (non-client) window theming. Windows uses DWM attributes for its
// caption/backdrop; macOS sets NSWindow.appearance to Aqua, Dark Aqua, or nil
// for the app's explicit light, explicit dark, or system preference. Other
// platforms keep these helpers as no-ops.
namespace NativeWindowTheme {

enum class BackdropMaterial {
    Mica,
    Acrylic,
};

// Successful DWM settings, owned by the window's chrome lifecycle.
struct AppliedState {
    std::optional<bool> darkMode;
    std::optional<int> backdropType;
    bool frameExtended = false;
};

// Updates appearance without rebuilding the window's backdrop.
void applyAppearanceToWindow(QWindow* window, AppliedState* state = nullptr);

// QWindow variant for QML / QQuickWindow top-levels. Frame geometry belongs
// to the window chrome; this helper only sets appearance and backdrop.
// Returns whether the requested native backdrop was accepted.
bool applyToWindow(QWindow* window, bool backdropEnabled = true,
                   BackdropMaterial material = BackdropMaterial::Mica,
                   AppliedState* state = nullptr);

}  // namespace NativeWindowTheme
