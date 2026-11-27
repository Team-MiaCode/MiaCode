import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

// Shared push button — geometry mirrors v1 dialogPushButtonStyleSheet.
Button {
    id: root

    property bool emphasized: false
    property bool destructive: false
    property bool selected: false
    property url iconSource
    property int iconSize: 16
    property string tooltip: ""
    property real minimumButtonWidth: 92
    property alias backgroundFillsWidth: chrome.fillWidth

    readonly property bool filled: root.emphasized || root.destructive
    readonly property bool hasIcon: root.iconSource.toString().length > 0
    readonly property color foregroundColor: !root.enabled ? Theme.colors.text.disabled
        : root.destructive ? Theme.colors.text.onDanger
        : root.emphasized ? Theme.colors.text.onAccent
        : (root.hovered || root.down || root.selected || root.checked || root.visualFocus)
            ? Theme.colors.text.active : Theme.colors.text.primary

    font.family: Theme.uiFont
    font.pixelSize: Theme.uiFontSize
    font.weight: root.filled ? Font.DemiBold : Font.Normal
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    implicitHeight: Theme.controlMinHeight
    implicitWidth: Math.max(root.minimumButtonWidth, contentItem.implicitWidth + leftPadding + rightPadding)
    Layout.preferredHeight: implicitHeight
    Layout.maximumHeight: implicitHeight
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: root.tooltip.length > 0 && root.text.length === 0 ? root.tooltip : root.text

    contentItem: RowLayout {
        spacing: root.hasIcon && root.text.length > 0 ? 6 : 0
        transformOrigin: Item.Center
        scale: root.enabled && root.down && Theme.animationsEnabled ? 0.96 : 1.0

        Behavior on scale {
            enabled: Theme.animationsEnabled
            NumberAnimation { duration: Theme.iconPressDuration; easing.type: Easing.OutCubic }
        }

        ControlsImpl.IconImage {
            Layout.preferredWidth: root.iconSize
            Layout.preferredHeight: root.iconSize
            Layout.alignment: Qt.AlignVCenter
            visible: root.hasIcon
            source: root.iconSource
            sourceSize: Theme.iconSourceSize(root.iconSize, root.iconSize)
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

    background: HoverChrome {
        id: chrome
        emphasizedFill: root.enabled && root.filled
        baseColor: !root.enabled ? Theme.colors.background.controlDisabled
                   : root.destructive ? Theme.colors.danger.primary
                   : root.emphasized ? Theme.colors.accent.primary
                   : Theme.colors.background.elevated
        borderColor: !root.enabled ? Theme.colors.border.normal
                   : root.filled ? "transparent"
                   : root.selected || root.checked ? Theme.colors.accent.primary
                   : Theme.floatingBorderColor
        borderWidth: root.filled && root.enabled ? 0 : 1 / root.Screen.devicePixelRatio
        stateColors: root.destructive ? Theme.colors.dangerState
                   : root.emphasized ? Theme.colors.accentState
                   : Theme.colors.buttonState
        selected: root.selected || root.checked
        hovered: root.hovered
        pressed: root.down
        focused: root.visualFocus
    }

    Tooltip {
        visible: root.hovered && root.tooltip.length > 0
        text: root.tooltip
    }
}
