import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

ColumnLayout {
    id: root
    required property string label
    required property var options
    required property string currentValue
    required property bool darkPalette
    readonly property var paletteOptions: options.filter(function(option) {
        const theme = Theme.themeCatalog[option.value]
        return theme && theme.dark === root.darkPalette
    })
    signal picked(string value)
    Layout.fillWidth: true
    spacing: 8

    Text {
        text: root.label
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
    }

    GridLayout {
        Layout.fillWidth: true
        columns: width >= 720 ? 4 : 2
        uniformCellWidths: true
        columnSpacing: 8
        rowSpacing: 8

        Repeater {
            model: root.paletteOptions
            delegate: AbstractButton {
                id: tile
                required property var modelData
                readonly property bool selected: root.currentValue === modelData.value
                readonly property var previewColors: (Theme.themeCatalog[modelData.value] || Theme.themeCatalog.dark).colors
                Layout.fillWidth: true
                Layout.minimumWidth: 100
                implicitHeight: 124
                padding: 8
                hoverEnabled: true
                objectName: "themeTile_" + modelData.value
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.label
                Accessible.checked: selected
                onClicked: root.picked(modelData.value)

                background: Rectangle {
                    radius: Theme.controlRadius
                    color: tile.hovered ? tile.previewColors.buttonState.hover : tile.previewColors.background.panel
                    border.width: tile.selected || tile.visualFocus ? 2 : 1
                    border.color: tile.selected || tile.visualFocus ? tile.previewColors.accent.primary : tile.previewColors.border.normal
                }
                contentItem: ColumnLayout {
                    spacing: 6
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 3
                        color: tile.previewColors.background.surface
                        clip: true
                        Rectangle {
                            width: 15
                            height: parent.height
                            color: tile.previewColors.background.panel
                            Rectangle {
                                width: 3
                                height: 14
                                y: 8
                                color: tile.previewColors.accent.primary
                            }
                        }
                        Column {
                            x: 24
                            y: 7
                            width: parent.width - x - 8
                            spacing: 3
                            Text {
                                width: parent.width
                                textFormat: Text.StyledText
                                wrapMode: Text.WrapAnywhere
                                text: {
                                    const syntax = tile.previewColors.syntax
                                    const note = syntax.note || tile.previewColors.text.editor
                                    const beat = syntax.beat || syntax.keyword
                                    return "<font color='" + syntax.keyword + "'>(180)</font>"
                                        + "<font color='" + beat + "'>{4}</font>"
                                        + "<font color='" + note + "'>1</font>,"
                                        + "<font color='" + note + "'>2x-5</font>"
                                        + "<font color='" + syntax.duration + "'>[8:1]</font>,"
                                        + "<font color='" + note + "'>3b</font>,"
                                        + "<font color='" + note + "'>4h</font>"
                                        + "<font color='" + syntax.duration + "'>[4:1]</font>,"
                                }
                                font.family: Theme.codeFont.family
                                font.pixelSize: 12
                                color: tile.previewColors.text.editor
                            }
                            Text {
                                objectName: "themeComment_" + tile.modelData.value
                                width: parent.width
                                text: "|| " + qsTrId("dialog.preferences.theme.comment_sample")
                                elide: Text.ElideRight
                                font.family: Theme.codeFont.family
                                font.pixelSize: 11
                                color: tile.previewColors.syntax.comment
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            objectName: "themeLabel_" + tile.modelData.value
                            text: tile.modelData.label
                            color: tile.previewColors.text.active
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.secondaryFontSize
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            width: 7
                            height: 7
                            radius: 4
                            visible: tile.selected
                            color: tile.previewColors.accent.primary
                        }
                    }
                }
            }
        }
    }
}
