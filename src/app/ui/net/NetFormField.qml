import QtQuick
import QtQuick.Layouts
import MiaCode.UI

ColumnLayout {
    id: root

    required property string label
    default property alias controls: controlRow.data

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    spacing: 4

    Text {
        Layout.fillWidth: true
        text: root.label
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        elide: Text.ElideRight
    }

    RowLayout {
        id: controlRow
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 6
    }
}
