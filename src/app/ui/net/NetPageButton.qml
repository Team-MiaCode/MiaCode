import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

AppButton {
    id: root

    property url iconSource
    property string tooltip: ""
    readonly property var interactionColors: destructive ? Theme.colors.dangerState
        : emphasized ? Theme.colors.accentState : Theme.colors.buttonState
    readonly property color foregroundColor: !enabled ? Theme.colors.text.disabled
        : destructive ? Theme.colors.text.onDanger
        : emphasized ? Theme.colors.text.onAccent : Theme.colors.text.primary

    leftPadding: 10
    rightPadding: 10
    implicitWidth: Math.max(Theme.controlMinHeight,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    Accessible.name: tooltip.length > 0 ? tooltip : text
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.text: tooltip

    contentItem: RowLayout {
        spacing: 6

        ControlsImpl.IconImage {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            visible: root.iconSource.toString().length > 0
            source: root.iconSource
            sourceSize: Theme.iconSourceSize(16, 16)
            color: root.foregroundColor
        }
        Text {
            Layout.fillWidth: true
            visible: root.text.length > 0
            text: root.text
            font: root.font
            color: root.foregroundColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        anchors.fill: parent
        anchors.topMargin: Theme.chromeInsetY
        anchors.bottomMargin: Theme.chromeInsetY
        radius: Theme.controlRadius
        color: Theme.surfaceColor(!root.enabled ? Theme.colors.background.controlDisabled
            : root.down ? root.interactionColors.pressed
            : root.hovered ? root.interactionColors.hover
            : root.checked || root.selected ? root.interactionColors.selected
            : root.destructive ? Theme.colors.danger.primary
            : root.emphasized ? Theme.colors.accent.primary
            : Theme.colors.background.control)
        border.width: 1 / root.Screen.devicePixelRatio
        border.color: root.enabled && (root.filled || root.visualFocus || root.checked || root.selected)
            ? root.destructive ? Theme.colors.danger.primary : Theme.colors.accent.primary
            : Theme.colors.border.normal
    }
}
