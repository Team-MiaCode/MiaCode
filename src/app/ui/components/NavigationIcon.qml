import QtQuick
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Item {
    id: root

    property url source
    property url filledSource
    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property color color: Theme.colors.text.secondary
    property string imageObjectName
    readonly property bool hasFilledSource: filledSource.toString().length > 0
    property real fillProgress: selected && hasFilledSource ? 1 : 0

    scale: !enabled ? 1 : pressed ? 0.9 : hovered ? 1.08 : 1

    Behavior on scale {
        enabled: Theme.animationsEnabled && root.enabled
        NumberAnimation {
            duration: root.pressed ? Theme.iconPressDuration : Theme.iconTransitionDuration
            easing.type: Easing.OutCubic
        }
    }
    Behavior on color {
        enabled: Theme.animationsEnabled
        ColorAnimation { duration: Theme.iconTransitionDuration }
    }
    Behavior on fillProgress {
        enabled: Theme.animationsEnabled
        NumberAnimation {
            duration: Theme.iconTransitionDuration
            easing.type: Easing.OutCubic
        }
    }

    // Both variants keep the same bounds throughout the selection transition.
    ControlsImpl.IconImage {
        objectName: root.imageObjectName
        anchors.fill: parent
        source: root.source
        sourceSize: Theme.iconSourceSize(root.width, root.height)
        color: root.color
        opacity: 1 - root.fillProgress
    }
    ControlsImpl.IconImage {
        anchors.fill: parent
        source: root.filledSource
        sourceSize: Theme.iconSourceSize(root.width, root.height)
        color: root.color
        opacity: root.fillProgress
        visible: root.hasFilledSource
    }
}
