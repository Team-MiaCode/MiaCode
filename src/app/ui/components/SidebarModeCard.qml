pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import MiaCode.UI

AbstractButton {
    id: card

    required property bool classic
    required property bool selected
    readonly property string layoutLabel: classic
        ? qsTrId("dialog.preferences.sidebar_mode.efficient")
        : qsTrId("dialog.preferences.sidebar_mode.standard")

    implicitWidth: 200
    implicitHeight: (width - 24) * 10 / 16 + 46
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: qsTrId("dialog.preferences.sidebar_mode") + ": " + layoutLabel
    Accessible.checkable: true
    Accessible.checked: selected

    background: Rectangle {
        radius: Theme.controlRadius
        color: Theme.overlayColor(card.selected ? Theme.colors.state.selected
             : card.down ? Theme.colors.state.pressed
             : card.hovered ? Theme.colors.state.hover : Theme.colors.background.control)
        border.width: card.selected || card.visualFocus ? 2 : 1
        border.color: card.selected || card.visualFocus
            ? Theme.colors.accent.primary : Theme.colors.border.control
    }

    contentItem: Item {
        Rectangle {
            id: miniature
            x: 12
            y: 12
            width: parent.width - 24
            height: width * 10 / 16
            radius: Theme.compactControlRadius
            color: Theme.surfaceColor(Theme.colors.background.surface)
            border.width: 1
            border.color: Theme.colors.border.normal

            Rectangle {
                id: navigation
                x: 6
                y: 6
                width: miniature.width * 0.2
                height: miniature.height - 12
                radius: 3
                color: Theme.colors.border.control

                RectangularShadow {
                    anchors.fill: parent
                    z: -1
                    radius: 3
                    blur: 6
                    offset: Qt.vector2d(0, 1)
                    color: Qt.rgba(0, 0, 0, Theme.popupShadowOpacity)
                }

                Item {
                    id: rail
                    visible: !card.classic
                    width: 9
                    height: parent.height

                    Rectangle {
                        x: 1
                        y: 1
                        width: parent.width - 1
                        height: parent.height - 2
                        radius: 2
                        color: Theme.colors.background.activityBar
                    }
                    Column {
                        x: 3
                        y: 8
                        spacing: 8
                        Repeater {
                            model: 3
                            delegate: Rectangle {
                                width: 4
                                height: 4
                                radius: 1
                                color: Theme.colors.text.secondary
                            }
                        }
                    }
                    Rectangle {
                        x: 3
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 6
                        width: 4
                        height: 4
                        radius: 1
                        color: Theme.colors.text.secondary
                    }
                }

                Item {
                    id: panel
                    x: card.classic ? 0 : rail.width
                    width: navigation.width - x
                    height: navigation.height

                    Rectangle {
                        x: 4; y: 8
                        width: 10; height: 2
                        radius: 1
                        color: Theme.colors.text.secondary
                    }
                    Column {
                        x: 4; y: 20
                        spacing: 7
                        Repeater {
                            model: 2
                            delegate: Rectangle {
                                width: panel.width - 9
                                height: 2
                                radius: 1
                                color: Theme.colors.text.disabled
                            }
                        }
                    }
                    Rectangle {
                        x: 4; y: 43
                        width: 8; height: 2
                        radius: 1
                        color: Theme.colors.text.secondary
                    }
                    Rectangle {
                        x: card.classic ? 4 : 8
                        y: 52
                        width: panel.width - x - 5
                        height: 2
                        radius: 1
                        color: Theme.colors.text.disabled
                    }
                    Column {
                        visible: card.classic
                        x: 4; y: 65
                        spacing: 7
                        Repeater {
                            model: 2
                            delegate: Rectangle {
                                width: panel.width - 9
                                height: 2
                                radius: 1
                                color: Theme.colors.text.disabled
                            }
                        }
                    }
                    Item {
                        visible: card.classic
                        x: 4
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 5
                        width: parent.width - 9
                        height: 3
                        Rectangle {
                            width: 3; height: 3
                            radius: 1
                            color: Theme.colors.text.secondary
                        }
                        Rectangle {
                            x: 6; y: 1
                            width: parent.width - 6
                            height: 2
                            radius: 1
                            color: Theme.colors.text.disabled
                        }
                    }
                }
            }

            Grid {
                id: statistics
                x: parent.width * 0.38
                y: navigation.y + navigation.height - height
                width: parent.width - x - 12
                columns: 3
                columnSpacing: 3
                rowSpacing: 5

                Repeater {
                    model: 3
                    delegate: Item {
                        id: statistic
                        width: (statistics.width - 2 * statistics.columnSpacing) / 3
                        height: card.classic ? 12 : 6

                        Rectangle {
                            visible: card.classic
                            anchors.fill: parent
                            radius: 3
                            color: Theme.overlayColor(Theme.colors.background.control)
                            border.width: 1
                            border.color: Theme.colors.border.control
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width * 0.55
                                height: 1
                                radius: 1
                                color: Theme.colors.text.disabled
                            }
                        }
                        Item {
                            visible: !card.classic
                            anchors.fill: parent
                            Rectangle {
                                x: 0
                                y: 1
                                width: 3
                                height: 3
                                radius: 1
                                color: Theme.colors.text.disabled
                            }
                            Rectangle {
                                x: 7
                                y: 0
                                width: statistic.width - 9
                                height: 1
                                color: Theme.colors.text.disabled
                            }
                            Rectangle {
                                x: 7
                                y: 5
                                width: (statistic.width - 9) * 0.6
                                height: 1
                                color: Theme.colors.text.secondary
                            }
                        }
                    }
                }
            }

        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 9
            text: card.layoutLabel
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            color: card.selected ? Theme.colors.text.active : Theme.colors.text.secondary
        }
    }
}
