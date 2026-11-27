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
        x: root.width - root.rightPadding - width
        y: (root.height - height) / 2

        Rectangle {
            anchors.fill: parent
            radius: Theme.smallControlRadius
            color: "transparent"
            border.width: 1
            border.color: root.checkState !== Qt.Unchecked ? Theme.colors.accent.muted
                : root.hovered ? Theme.colors.text.secondary
                : Theme.colors.toggle.border

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: Theme.overlayColor(Theme.colors.accent.muted)
                opacity: root.checkState !== Qt.Unchecked ? 1 : 0
                visible: opacity > 0.005
                Behavior on opacity {
                    enabled: Theme.animationsEnabled && root.visible
                    NumberAnimation { duration: Theme.hoverTransitionDuration; easing.type: Easing.OutCubic }
                }
            }
        }

        ControlsImpl.IconImage {
            anchors.centerIn: parent
            width: root.compact ? 9 : 11
            height: root.compact ? 9 : 11
            opacity: root.checkState === Qt.Checked ? 1 : 0
            scale: root.checkState === Qt.Checked ? 1 : 0.72
            visible: opacity > 0.005
            source: "qrc:/icons/checkmark.svg"
            sourceSize: Theme.iconSourceSize(width, height)
            color: Theme.colors.toggle.checkedKnob
            Behavior on opacity {
                enabled: Theme.animationsEnabled && root.visible
                NumberAnimation { duration: Theme.hoverTransitionDuration; easing.type: Easing.OutCubic }
            }
            Behavior on scale {
                enabled: Theme.animationsEnabled && root.visible
                NumberAnimation { duration: Theme.hoverTransitionDuration; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.compact ? 6 : 8
            height: 2
            radius: height / 2
            visible: root.checkState === Qt.PartiallyChecked
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

        rightPadding: root.indicator.width + root.spacing
        text: root.text
        font: root.font
        color: root.enabled ? Theme.colors.text.primary : Theme.colors.text.disabled
        verticalAlignment: Text.AlignVCenter
    }
}
