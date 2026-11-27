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
    property real iconRotation: 0
    property bool horizontal: false
    property string tooltip: ""
    property string accessibleName: tooltip
    property string buttonObjectName: ""
    property bool interactive: true
    readonly property alias hovered: pill.hovered
    readonly property alias pressed: pill.down
    readonly property alias visualFocus: pill.visualFocus

    signal clicked()

    implicitWidth: root.horizontal ? 44 : 22
    implicitHeight: root.horizontal ? 22 : 44

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
        radius: Math.min(width, height) / 2
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
        focusPolicy: Qt.TabFocus
        enabled: root.interactive
        Accessible.name: root.accessibleName

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        onClicked: root.clicked()

        background: HoverChrome {
            fillBounds: true
            highlightOutset: 0
            cornerRadius: Math.min(width, height) / 2
            baseColor: Theme.colors.background.elevated
            stateColors: Theme.colors.buttonState
            hovered: pill.hovered
            pressed: pill.down
            focused: pill.visualFocus
            borderWidth: 1 / root.Screen.devicePixelRatio
            borderColor: Theme.colors.border.control
        }

        contentItem: Item {
            transformOrigin: Item.Center
            scale: pill.enabled && pill.down && Theme.animationsEnabled ? 0.90 : 1.0
            Behavior on scale {
                enabled: Theme.animationsEnabled
                NumberAnimation { duration: Theme.iconPressDuration; easing.type: Easing.OutCubic }
            }
            ControlsImpl.IconImage {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                rotation: root.iconRotation
                source: root.iconSource
                sourceSize: Theme.iconSourceSize(root.iconSize, root.iconSize)
                color: pill.hovered || pill.down || pill.visualFocus
                    ? Theme.colors.text.active : Theme.colors.text.secondary
            }
        }

        Tooltip {
            visible: pill.hovered && root.tooltip.length > 0
            text: root.tooltip
        }
    }
}
