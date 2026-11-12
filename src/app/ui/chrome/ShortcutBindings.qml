pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// Window-level shortcuts for v2.
//
// v1 binds these as QActions on MainWindow with Qt::WindowShortcut context. In
// v2 that window is hidden and therefore never active, so none of them ever
// fired — the chart transforms had no keyboard route at all. Binding them here
// puts them on the window the user is actually typing into.
//
// Sequences come from ShortcutRegistry, the same table the Preferences editor
// writes, so a customized binding applies to v1 and v2 alike. The id list comes
// from the command service rather than being retyped here.
Item {
    id: root

    required property var shortcuts
    required property var commands
    required property var previewSession
    property var timelineStateBridge: null
    property var inputContext: null
    readonly property string zoomInGesture: timelineStateBridge && shortcuts.revision >= 0
        ? shortcuts.gestureText("timeline.zoom_in", "Ctrl+WheelUp") : ""
    readonly property string zoomOutGesture: timelineStateBridge && shortcuts.revision >= 0
        ? shortcuts.gestureText("timeline.zoom_out", "Ctrl+WheelDown") : ""

    function syncTimelineGestures() {
        if (timelineStateBridge)
            timelineStateBridge.setZoomWheelShortcuts([zoomInGesture], [zoomOutGesture])
    }
    onZoomInGestureChanged: syncTimelineGestures()
    onZoomOutGestureChanged: syncTimelineGestures()
    onTimelineStateBridgeChanged: syncTimelineGestures()
    Component.onCompleted: syncTimelineGestures()
    // Transforms edit the chart, so they are inert without one.
    property bool commandsEnabled: true
    property bool chartCommandsEnabled: true
    property bool playbackCommandsEnabled: true
    property bool menuOwnsChartTransformShortcuts: false
    property bool menuOwnsPreviewRateShortcuts: false

    // A transform acts on the editor's selection, which only the editor knows.
    // Routing it through the backend meant reading a hidden widget's cursor
    // that nothing ever synced from here, so every transform found an empty
    // selection and silently did nothing.
    signal chartTransformRequested(string opId)

    Shortcut {
        sequence: root.timelineStateBridge && root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("timeline.zoom_in") : ""
        enabled: root.commandsEnabled && root.chartCommandsEnabled
            && !root.shortcuts.captureActive && sequence !== ""
        context: Qt.WindowShortcut
        onActivated: root.timelineStateBridge.stepZoomIn()
    }

    Shortcut {
        sequence: root.timelineStateBridge && root.shortcuts.revision >= 0
            ? root.shortcuts.sequence("timeline.zoom_out") : ""
        enabled: root.commandsEnabled && root.chartCommandsEnabled
            && !root.shortcuts.captureActive && sequence !== ""
        context: Qt.WindowShortcut
        onActivated: root.timelineStateBridge.stepZoomOut()
    }

    Instantiator {
        model: root.commands.shortcutCommandIds()
        delegate: Shortcut {
            required property string modelData
            // Re-resolved when the registry reloads.
            sequence: root.shortcuts.revision >= 0
                ? root.shortcuts.sequence(modelData)
                : ""
            enabled: root.commandsEnabled && root.chartCommandsEnabled
                     && !root.shortcuts.captureActive
                     && !root.menuOwnsChartTransformShortcuts
                     && sequence !== ""
            context: Qt.WindowShortcut
            onActivated: root.chartTransformRequested(modelData)
        }
    }

    PreviewShortcuts {
        shortcuts: root.shortcuts
        previewSession: root.previewSession
        inputContext: root.inputContext
        commandsEnabled: root.commandsEnabled
        playbackCommandsEnabled: root.playbackCommandsEnabled
        menuOwnsPreviewRateShortcuts: root.menuOwnsPreviewRateShortcuts
    }
}
