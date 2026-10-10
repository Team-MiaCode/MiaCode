import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

CheckBox {
    id: root

    property bool compact: false

    spacing: compact ? 4 : 6
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    implicitHeight: 24
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    font.family: Theme.uiFont
    font.pixelSize: compact ? Theme.compactFontSize : Theme.secondaryFontSize
    indicator: Item {
        implicitWidth: root.compact ? 13 : 15
        implicitHeight: root.compact ? 13 : 15
        x: root.leftPadding
        y: (root.height - height) / 2

        Rectangle {
            anchors.fill: parent
            radius: Theme.smallControlRadius
            // Same on/off colors as AppSwitch.
            color: Theme.overlayColor(root.checked ? Theme.colors.toggle.checkedTrack : "transparent")
            border.width: 1
            border.color: root.checked ? Theme.colors.toggle.checkedTrack
                : root.hovered ? Theme.colors.text.secondary
                : Theme.colors.toggle.border
        }

        ControlsImpl.IconImage {
            anchors.centerIn: parent
            width: root.compact ? 9 : 11
            height: root.compact ? 9 : 11
            visible: root.checked
            source: "qrc:/icons/checkmark.svg"
            sourceSize: Qt.size(width, height)
            color: Theme.colors.toggle.checkedKnob
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            visible: root.visualFocus
            radius: Theme.smallControlRadius + 2
            color: "transparent"
            border.width: 1
            border.color: Theme.colors.accent.focus
        }
    }

    contentItem: Text {
        id: label

        leftPadding: root.indicator.width + root.spacing
        text: root.text
        font: root.font
        color: root.enabled ? Theme.colors.text.primary : Theme.colors.text.disabled
        verticalAlignment: Text.AlignVCenter
    }
}
