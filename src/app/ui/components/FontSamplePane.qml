import QtQuick
import QtQuick.Layouts
import MiaCode.UI

Rectangle {
    id: root

    default property alias content: samples.data

    Layout.fillWidth: true
    implicitHeight: samples.implicitHeight + 2 * Theme.panelPadding
    radius: Theme.controlRadius
    color: Theme.surfaceColor(Theme.colors.background.surface)
    border.color: Theme.colors.border.normal

    ColumnLayout {
        id: samples
        anchors.fill: parent
        anchors.margins: Theme.panelPadding
        spacing: 4
    }
}
