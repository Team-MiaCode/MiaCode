pragma ComponentBehavior: Bound

import QtQuick

// Install once in each top-level window. The active window owns the key
// dispatch; the shared preferences model applies its scale to every window.
Item {
    id: root

    required property var shortcuts
    required property var preferencesModel
    property bool commandsEnabled: true

    function adjustScale(delta) {
        root.preferencesModel.uiScalePercent = Math.max(75,
            Math.min(200, root.preferencesModel.uiScalePercent + delta))
    }

    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("ui.zoom_out", "Ctrl+Alt+-") : ""
        enabled: root.commandsEnabled && !root.shortcuts.captureActive && sequence !== ""
        context: Qt.WindowShortcut
        onActivated: root.adjustScale(-10)
    }

    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("ui.zoom_in", "Ctrl+Alt+=") : ""
        enabled: root.commandsEnabled && !root.shortcuts.captureActive && sequence !== ""
        context: Qt.WindowShortcut
        onActivated: root.adjustScale(10)
    }

    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("ui.zoom_reset", "Ctrl+Alt+0") : ""
        enabled: root.commandsEnabled && !root.shortcuts.captureActive && sequence !== ""
        context: Qt.WindowShortcut
        onActivated: root.preferencesModel.uiScalePercent = 100
    }
}
