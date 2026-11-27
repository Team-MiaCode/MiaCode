pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import MiaCode.UI

AbstractButton {
    id: card

    required property bool previewOnLeft
    required property bool selected
    readonly property string layoutLabel: previewOnLeft
        ? qsTrId("dialog.preferences.layout.preview_editor")
        : qsTrId("dialog.preferences.layout.editor_preview")

    implicitWidth: 180
    implicitHeight: (width - 24) * 10 / 16 + 46
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: qsTrId("dialog.preferences.interface_layout") + ": " + layoutLabel
    Accessible.checkable: true
    Accessible.checked: selected

    background: HoverChrome {
        fillBounds: true
        cornerRadius: Theme.controlRadius
        baseColor: Theme.colors.background.control
        stateColors: Theme.colors.buttonState
        selected: card.selected
        hovered: card.hovered
        pressed: card.down
        focused: card.visualFocus
        borderWidth: card.selected || card.visualFocus ? 2 : 1
        borderColor: card.selected ? Theme.colors.accent.primary
            : card.hovered ? Theme.colors.accent.muted : Theme.colors.border.control
    }

    contentItem: Item {
        transformOrigin: Item.Center
        scale: card.enabled && card.down && Theme.animationsEnabled ? 0.985 : 1.0
        Behavior on scale {
            enabled: Theme.animationsEnabled
            NumberAnimation { duration: Theme.iconPressDuration; easing.type: Easing.OutCubic }
        }
        Rectangle {
            id: miniature
            readonly property real contentLeft: miniatureSidebar.x + miniatureSidebar.width + 4
            readonly property real contentWidth: width - contentLeft - 5
            readonly property real paneSpacing: 8
            readonly property real paneWidth: (contentWidth - paneSpacing) / 2
            x: 12
            y: 12
            // Keep the miniature in design units and scale its complete scene.
            width: 176
            height: width * 10 / 16
            scale: Math.max(0, parent.width - 24) / width
            transformOrigin: Item.TopLeft
            radius: Theme.compactControlRadius
            color: Theme.surfaceColor(Theme.colors.background.surface)
            border.width: 1
            border.color: Theme.colors.border.normal

            Rectangle {
                x: 5; y: 5
                width: parent.width - 10
                height: 5
                radius: 2
                color: Theme.colors.border.control
            }
            Rectangle {
                id: miniatureSidebar
                x: 5; y: 14
                width: miniature.width * 0.14
                height: parent.height - 19
                radius: 2
                color: Theme.colors.border.control
            }

            Item {
                id: previewPane
                x: card.previewOnLeft ? miniature.contentLeft : miniature.width - width - 5
                y: 14
                width: miniature.paneWidth
                height: miniature.height - 19

                Item {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: previewControls.top
                    anchors.bottomMargin: 4

                    Rectangle {
                        id: previewCircle
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 8, parent.height - 8)
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.colors.accent.primary
                    }
                }
                Grid {
                    id: previewControls
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: previewCircle.width
                    columns: 3
                    spacing: 3

                    Repeater {
                        model: 3
                        delegate: Item {
                            width: (previewControls.width - 6) / 3
                            height: 5

                            Rectangle {
                                width: 3; height: 3
                                y: 1
                                radius: 1
                                color: Theme.colors.text.disabled
                            }
                            Rectangle {
                                x: 5; y: 0
                                width: parent.width - 5
                                height: 1
                                color: Theme.colors.text.disabled
                            }
                            Rectangle {
                                x: 5; y: 4
                                width: (parent.width - 5) * 0.6
                                height: 1
                                color: Theme.colors.text.secondary
                            }
                        }
                    }
                }
            }

            Item {
                id: editorPane
                x: card.previewOnLeft ? miniature.width - width - 5 : miniature.contentLeft
                y: 14
                width: miniature.paneWidth
                height: miniature.height - 19

                Column {
                    y: 6
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 5
                    Repeater {
                        model: [0.85, 0.6, 0.75, 0.5]
                        delegate: Rectangle {
                            required property real modelData
                            width: editorPane.width * modelData
                            height: 2
                            radius: 1
                            color: Theme.colors.text.disabled
                        }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 18
                    radius: 2
                    color: Theme.colors.border.control

                    Repeater {
                        model: 5
                        delegate: Rectangle {
                            required property int index
                            x: (index + 1) * parent.width / 6
                            y: 3
                            width: 1
                            height: parent.height - 6
                            color: Theme.colors.text.disabled
                        }
                    }
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            text: card.layoutLabel
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            color: card.selected ? Theme.colors.text.active : Theme.colors.text.secondary
        }
    }
}
