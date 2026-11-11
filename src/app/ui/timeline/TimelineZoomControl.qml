pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Window
import MiaCode.UI

Control {
    id: root

    required property var stateBridge

    readonly property var presets: root.stateBridge ? root.stateBridge.zoomPresetValues : []
    readonly property int presetIndex: {
        if (root.stateBridge) {
            for (let i = 0; i < root.presets.length; ++i) {
                if (Math.abs(root.presets[i] - root.stateBridge.zoomScale) < 1e-6)
                    return i
            }
        }
        return -1
    }
    readonly property real horizontalInset: Theme.chromeInsetX - Theme.chromeHighlightOutset
    readonly property real verticalInset: Theme.chromeInsetY - Theme.chromeHighlightOutset
    readonly property real stepperWidth: Theme.compactControlHeight - 2
    readonly property real strokeWidth: 1 / root.Screen.devicePixelRatio
    readonly property real bodyWidth: Math.max(54,
        Math.ceil(labelProbe.implicitWidth) + 4 * Theme.chromePadding)
    property real wheelRemainder: 0

    implicitWidth: bodyWidth + stepperWidth + 2 * horizontalInset
    implicitHeight: Theme.controlHighlightHeight + 2 * verticalInset
    enabled: presetIndex >= 0
    font.family: Theme.uiFont
    font.pixelSize: Theme.captionFontSize
    leftPadding: horizontalInset + strokeWidth
    rightPadding: leftPadding
    topPadding: verticalInset + strokeWidth
    bottomPadding: topPadding

    function stepPresets(steps) {
        if (!root.enabled || steps === 0)
            return
        const next = Math.max(0, Math.min(root.presets.length - 1, root.presetIndex + steps))
        if (next !== root.presetIndex)
            root.stateBridge.applyZoomPreset(root.presets[next])
    }

    // Text's implicit width notifies when its font or translation changes;
    // calling FontMetrics.advanceWidth alone does not create that dependency.
    Text {
        id: labelProbe
        visible: false
        font: root.font
        text: {
            const labels = []
            for (let i = 0; i < root.presets.length; ++i)
                labels.push(qsTrId("qml.1").arg(Math.round(root.presets[i] * 100)))
            return labels.join("\n")
        }
    }

    Keys.onUpPressed: event => {
        if (event.modifiers !== Qt.NoModifier) {
            event.accepted = false
            return
        }
        root.stepPresets(1)
    }
    Keys.onDownPressed: event => {
        if (event.modifiers !== Qt.NoModifier) {
            event.accepted = false
            return
        }
        root.stepPresets(-1)
    }

    background: Rectangle {
        id: chrome
        x: root.horizontalInset
        y: root.verticalInset
        width: root.width - 2 * root.horizontalInset
        height: root.height - 2 * root.verticalInset
        radius: Theme.controlRadius
        color: Theme.surfaceColor(Theme.colors.background.elevated)
        border.color: Theme.floatingBorderColor
        border.width: root.strokeWidth
        border.pixelAligned: false
    }

    contentItem: Item {
        // Keep the three input regions inside the shared border. Half-height
        // arrow buttons cannot use HoverChrome's minimum control height.

        AbstractButton {
            id: valueButton
            objectName: "timelineZoomValueButton"
            width: parent.width - root.stepperWidth
            height: parent.height
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.name: qsTrId("qml.timeline_zoom")
            Accessible.description: qsTrId("qml.open_timeline_zoom_presets")
            contentItem: Text {
                id: valueLabel
                text: qsTrId("qml.1").arg(Math.round(root.stateBridge ? root.stateBridge.zoomScale * 100 : 50))
                font: root.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: !root.enabled ? Theme.colors.text.disabled
                    : valueButton.hovered || valueButton.visualFocus || zoomMenu.active
                        ? Theme.colors.text.active : Theme.colors.text.primary
            }
            background: Rectangle {
                topLeftRadius: Theme.controlRadius - root.strokeWidth
                bottomLeftRadius: topLeftRadius
                color: Theme.overlayColor(valueButton.down ? Theme.colors.buttonState.pressed
                    : valueButton.hovered ? Theme.colors.buttonState.hover
                    : valueButton.visualFocus || zoomMenu.active ? Theme.colors.buttonState.selected
                    : "transparent")
            }
            onClicked: {
                if (zoomMenu.active)
                    zoomMenu.close()
                else
                    zoomMenu.openAt(root)
            }
            Tooltip {
                visible: valueButton.hovered
                text: qsTrId("qml.timeline_zoom")
            }
        }

        component StepButton: AbstractButton {
            id: stepButton
            required property bool increase
            readonly property string tooltip: qsTrId(increase ? "qml.zoom_in" : "qml.zoom_out")
            x: valueButton.width + root.strokeWidth
            width: parent.width - x
            height: parent.height / 2
            y: increase ? 0 : height
            enabled: increase ? root.presetIndex < root.presets.length - 1 : root.presetIndex > 0
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.name: tooltip
            contentItem: Item {
                ControlsImpl.IconImage {
                    objectName: stepButton.increase ? "timelineZoomIncreaseIcon" : "timelineZoomDecreaseIcon"
                    // Preserve half-pixel centers when a step region has an
                    // odd size; rounded anchor positions offset these icons.
                    x: (parent.width - width) / 2
                    y: (parent.height - height) / 2
                    width: 10
                    height: 10
                    source: Qt.resolvedUrl("icons/chevron-down.svg")
                    sourceSize: Theme.iconSourceSize(width, height)
                    rotation: stepButton.increase ? 180 : 0
                    color: !stepButton.enabled ? Theme.colors.text.disabled
                        : stepButton.hovered || stepButton.visualFocus ? Theme.colors.text.active
                        : Theme.colors.text.secondary
                }
            }
            background: Rectangle {
                topRightRadius: stepButton.increase ? Theme.controlRadius - root.strokeWidth : 0
                bottomRightRadius: stepButton.increase ? 0 : Theme.controlRadius - root.strokeWidth
                color: !stepButton.enabled ? "transparent"
                    : Theme.overlayColor(stepButton.down ? Theme.colors.buttonState.pressed
                        : stepButton.hovered ? Theme.colors.buttonState.hover
                        : stepButton.visualFocus ? Theme.colors.buttonState.selected : "transparent")
            }
            onClicked: {
                zoomMenu.close()
                root.stepPresets(increase ? 1 : -1)
            }
            Tooltip {
                visible: stepButton.hovered
                text: stepButton.tooltip
            }
        }

        StepButton {
            objectName: "timelineZoomIncreaseButton"
            increase: true
        }
        StepButton {
            objectName: "timelineZoomDecreaseButton"
            increase: false
        }

        Rectangle {
            x: valueButton.width
            width: root.strokeWidth
            height: parent.height
            color: Theme.floatingBorderColor
        }
        Rectangle {
            x: valueButton.width + Theme.chromePadding
            y: (parent.height - height) / 2
            width: root.stepperWidth - 2 * Theme.chromePadding
            height: root.strokeWidth
            color: Theme.floatingBorderColor
        }
    }

    Timer {
        id: wheelReset
        interval: 250
        onTriggered: root.wheelRemainder = 0
    }
    WheelHandler {
        target: null
        acceptedModifiers: Qt.NoModifier
        onWheel: event => {
            const delta = event.angleDelta.y
            if (!delta) {
                event.accepted = false
                return
            }
            if (Math.sign(root.wheelRemainder) !== Math.sign(delta))
                root.wheelRemainder = 0
            root.wheelRemainder += delta
            wheelReset.restart()
            const steps = Math.trunc(root.wheelRemainder / 120)
            if (steps) {
                root.wheelRemainder -= steps * 120
                zoomMenu.close()
                root.stepPresets(steps)
            }
            event.accepted = true
        }
    }

    TimelineZoomMenu {
        id: zoomMenu
        objectName: "timelineZoomMenu"
        stateBridge: root.stateBridge
    }
}
