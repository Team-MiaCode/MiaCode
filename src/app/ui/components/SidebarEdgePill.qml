import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Effects
import QtQuick.Window
import MiaCode.UI

// Floating edge pill for collapsing a docked efficient sidebar or pinning a peek drawer.
Item {
    id: root

    property url iconSource
    property int iconSize: 13
    property string tooltip: ""
    property string accessibleName: tooltip
    property string buttonObjectName: ""
    property bool interactive: true
    readonly property alias hovered: pill.hovered
    readonly property alias pressed: pill.down
    readonly property alias visualFocus: pill.visualFocus

    signal clicked()

    implicitWidth: 22
    implicitHeight: 44

    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.AllButtons
        hoverEnabled: false
        onPressed: mouse => mouse.accepted = true
        onWheel: wheel => wheel.accepted = true
    }

    RectangularShadow {
        anchors.fill: pill
        radius: pill.width / 2
        blur: 12
        offset: Qt.vector2d(0, 2)
        color: Qt.rgba(0, 0, 0, Theme.darkTheme ? 0.36 : 0.14)
        visible: pill.visible
    }

    AbstractButton {
        id: pill
        objectName: root.buttonObjectName
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.interactive
        Accessible.name: root.accessibleName

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        onClicked: root.clicked()

        background: Rectangle {
            radius: width / 2
            color: Theme.overlayColor(pill.down
                ? Theme.colors.buttonState.pressed
                : pill.hovered
                    ? Theme.colors.buttonState.hover
                    : Theme.colors.background.elevated)
            border.width: pill.visualFocus
                ? Theme.controlBorderWidth : 1 / root.Screen.devicePixelRatio
            border.color: pill.visualFocus
                ? Theme.colors.accent.focus : Theme.colors.border.control
            border.pixelAligned: false
        }

        contentItem: Item {
            ControlsImpl.IconImage {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                source: root.iconSource
                sourceSize: Theme.iconSourceSize(root.iconSize, root.iconSize)
                color: pill.hovered || pill.visualFocus
                    ? Theme.colors.text.active : Theme.colors.text.secondary
            }
        }

        Tooltip {
            visible: pill.hovered && root.tooltip.length > 0
            text: root.tooltip
        }
    }
}
