import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

Item {
    id: root

    property string text
    property string secondaryText
    property url iconSource
    property url filledIconSource
    property int difficultyId: 0
    property string tooltip
    property bool active: false
    property bool closable: false
    property bool showSeparator: false
    property real preferredTabWidth: 160
    readonly property real outlineWidth: 1 / Screen.devicePixelRatio
    readonly property bool hovered: tabButton.hovered || closeButton.hovered

    signal clicked()
    signal closeRequested()

    implicitHeight: Theme.workspaceHeaderHeight
    implicitWidth: preferredTabWidth
    z: active ? 1 : 0

    AbstractButton {
        id: tabButton

        anchors.fill: parent
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: root.secondaryText.length > 0
            ? root.text + " " + root.secondaryText
            : root.text
        Accessible.description: root.tooltip
        onClicked: root.clicked()

        TapHandler {
            acceptedButtons: Qt.MiddleButton
            onTapped: {
                if (root.closable)
                    root.closeRequested()
            }
        }

        contentItem: Item {
            RowLayout {
                id: contentRow

                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 9
                spacing: 6

                DifficultySwatch {
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.difficultyId > 0
                    difficultyId: root.difficultyId
                }

                ControlsImpl.IconImage {
                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                    visible: root.difficultyId <= 0
                             && root.iconSource.toString().length > 0
                    source: root.active && root.filledIconSource.toString().length > 0
                        ? root.filledIconSource : root.iconSource
                    sourceSize: Qt.size(15, 15)
                    color: root.active ? Theme.colors.text.active : Theme.colors.text.secondary
                }

                Text {
                    id: label

                    Layout.fillWidth: true
                    text: root.text
                    elide: Text.ElideRight
                    color: root.active ? Theme.colors.text.active : Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.secondaryFontSize
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    Layout.preferredWidth: implicitWidth
                    visible: root.secondaryText.length > 0
                    text: root.secondaryText
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.secondaryFontSize
                    verticalAlignment: Text.AlignVCenter
                }

                AbstractButton {
                    id: closeButton

                    Layout.preferredWidth: root.closable ? 24 : 0
                    Layout.preferredHeight: 24
                    visible: root.closable
                    opacity: root.active || root.hovered || activeFocus ? 1 : 0
                    enabled: opacity > 0
                    hoverEnabled: true
                    focusPolicy: Qt.TabFocus
                    Accessible.name: qsTrId("qml.close_1").arg(root.text)
                    onClicked: root.closeRequested()

                    contentItem: ControlsImpl.IconImage {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        source: Qt.resolvedUrl("icons/close.svg")
                        sourceSize: Qt.size(14, 14)
                        color: Theme.colors.text.secondary
                    }
                    background: Rectangle {
                        radius: Theme.smallControlRadius
                        color: closeButton.down ? Theme.colors.buttonState.pressed
                            : closeButton.hovered ? Theme.colors.buttonState.hover : "transparent"
                        border.width: closeButton.visualFocus ? Theme.controlBorderWidth : 0
                        border.color: Theme.colors.accent.focus
                    }
                    Tooltip {
                        visible: closeButton.hovered
                        text: qsTrId("qml.close_ctrl_w")
                    }
                }
            }
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                radius: Theme.editorTabRadius
                visible: !root.active
                color: tabButton.down ? Theme.overlayColor(Theme.colors.state.pressed)
                    : root.hovered ? Theme.overlayColor(Theme.colors.state.hover) : "transparent"
            }

            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: root.outlineWidth
                height: Theme.uiFontSize + 3
                visible: root.showSeparator && !root.active && !root.hovered
                color: Theme.separatorColor
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Theme.controlBorderWidth * 2
                radius: Theme.smallControlRadius
                visible: tabButton.visualFocus
                color: "transparent"
                border.width: Theme.controlBorderWidth
                border.color: Theme.colors.accent.focus
            }
        }
    }

    Tooltip {
        visible: tabButton.hovered && !closeButton.hovered && root.tooltip.length > 0
        text: root.tooltip
    }
}
