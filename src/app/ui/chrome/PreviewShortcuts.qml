pragma ComponentBehavior: Bound

import QtQuick

// Install in each workbench window; each instance uses the shared registry.
Item {
    id: root
    required property var shortcuts
    required property var previewSession
    property var inputContext: null
    property bool commandsEnabled: true
    property bool playbackCommandsEnabled: true
    property bool menuOwnsPreviewRateShortcuts: false

    // Preview commands bind straight to the preview session instead of going
    // through the backend command table.
    // Runtime owns the stop/start decision, including pending startup.
    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("preview.stop_or_play", "Ctrl+Shift+C")
            : ""
        enabled: root.commandsEnabled && root.playbackCommandsEnabled
                 && !root.shortcuts.captureActive && sequence !== ""
        context: Qt.WindowShortcut
        autoRepeat: false
        onActivated: root.previewSession.stopOrPlay()
    }

    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("preview.play_pause_global", "Ctrl+Shift+X")
            : ""
        enabled: root.commandsEnabled && root.playbackCommandsEnabled
                 && !root.shortcuts.captureActive && sequence !== ""
        context: Qt.WindowShortcut
        autoRepeat: false
        onActivated: root.previewSession.togglePlayback()
    }

    // Defaults to plain Space, so it must stay out of typing, menus and dialogs.
    // Another command on the same keys wins instead of making Qt drop both.
    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("preview.play_pause_non_text")
            : ""
        enabled: root.commandsEnabled && root.playbackCommandsEnabled
                 && !root.shortcuts.captureActive && sequence !== ""
                 && (!root.inputContext || root.inputContext.nonTextCommandsAllowed)
                 && root.shortcuts.revision >= 0
                 && !root.shortcuts.overlapsOtherShortcut("preview.play_pause_non_text")
        context: Qt.WindowShortcut
        autoRepeat: false
        onActivated: root.previewSession.togglePlayback()
    }

    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("preview.speed_down", "Ctrl+O")
            : ""
        enabled: root.commandsEnabled && root.playbackCommandsEnabled
                 && !root.shortcuts.captureActive
                 && !root.menuOwnsPreviewRateShortcuts
                 && sequence !== ""
        context: Qt.WindowShortcut
        onActivated: root.previewSession.adjustRate(-1)
    }

    Shortcut {
        sequence: root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("preview.speed_up", "Ctrl+P")
            : ""
        enabled: root.commandsEnabled && root.playbackCommandsEnabled
                 && !root.shortcuts.captureActive
                 && !root.menuOwnsPreviewRateShortcuts
                 && sequence !== ""
        context: Qt.WindowShortcut
        onActivated: root.previewSession.adjustRate(1)
    }
}
