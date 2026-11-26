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
    property bool dragging: false
    property bool closable: false
    property bool showSeparator: false
    property real preferredTabWidth: 160
    readonly property real outlineWidth: 1 / Screen.devicePixelRatio
    readonly property bool hovered: tabButton.hovered || closeButton.hovered
    readonly property bool closeButtonPressed: closeButton.down

    signal clicked()
    signal closeRequested()

    implicitHeight: Theme.workspaceHeaderHeight
    implicitWidth: preferredTabWidth
    z: dragging ? 10 : active ? 1 : 0

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
                    sourceSize: Theme.iconSourceSize(15, 15)
                    color: root.active || root.dragging ? Theme.colors.text.active : Theme.colors.text.secondary

                    Behavior on color {
                        ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
                    }
                }

                Text {
                    id: label

                    Layout.fillWidth: true
                    text: root.text
                    elide: Text.ElideRight
                    color: root.active || root.dragging ? Theme.colors.text.active : Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.secondaryFontSize
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter

                    Behavior on color {
                        ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
                    }
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
                    objectName: "tabCloseButton"

                    Layout.preferredWidth: root.closable ? 24 : 0
                    Layout.preferredHeight: 24
                    visible: root.closable
                    opacity: root.active || root.hovered || root.dragging || activeFocus ? 1 : 0
                    enabled: (root.active || root.hovered || activeFocus) && !root.dragging
                    hoverEnabled: true
                    focusPolicy: Qt.TabFocus
                    Accessible.name: qsTrId("qml.close_1").arg(root.text)
                    onClicked: root.closeRequested()

                    Behavior on opacity {
                        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }

                    // Preserve the 14px glyph within the 24px hit/hover area,
                    // even when sourceSize requests more texture pixels.
                    contentItem: Item {
                        ControlsImpl.IconImage {
                            objectName: "tabCloseIcon"
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: Qt.resolvedUrl("icons/close.svg")
                            sourceSize: Theme.iconSourceSize(14, 14)
                            color: Theme.colors.text.secondary
                        }
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
                anchors.topMargin: root.dragging && !root.active ? 3 : 0
                anchors.bottomMargin: root.dragging && !root.active ? 3 : 0
                anchors.leftMargin: root.dragging && !root.active ? 2 : 0
                anchors.rightMargin: root.dragging && !root.active ? 2 : 0
                radius: Theme.editorTabRadius
                visible: !root.active
                color: root.dragging
                    ? Theme.surfaceColor(Theme.colors.background.surface)
                    : tabButton.down ? Theme.overlayColor(Theme.colors.state.pressed)
                    : root.hovered ? Theme.overlayColor(Theme.colors.state.hover) : "transparent"
                border.width: root.dragging ? root.outlineWidth : 0
                border.color: Theme.colors.accent.primary

                Behavior on color {
                    ColorAnimation { duration: 110; easing.type: Easing.OutCubic }
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: root.outlineWidth
                height: Theme.uiFontSize + 3
                opacity: root.showSeparator && !root.active && !root.hovered && !root.dragging ? 1 : 0
                color: Theme.separatorColor

                Behavior on opacity {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Theme.controlBorderWidth * 2
                radius: Theme.smallControlRadius
                visible: tabButton.visualFocus && !root.dragging
                color: "transparent"
                border.width: Theme.controlBorderWidth
                border.color: Theme.colors.accent.focus
            }
        }
    }

    Tooltip {
        visible: !root.dragging && tabButton.hovered && !closeButton.hovered && root.tooltip.length > 0
        text: root.tooltip
    }
}
