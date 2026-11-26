import QtQuick
import QtQuick.Layouts
import MiaCode.UI

// Sidebar / list navigation row. Text: selected → active (1.0); idle →
// secondary (~0.75 of active). Chrome and its content inset come from ChromeRow.
ChromeRow {
    id: root

    property int textLeftPadding: 20
    property bool emphasizedText: false
    property real textPixelSize: Theme.uiFontSize
    property url iconSource
    property url filledIconSource
    readonly property bool hasIcon: iconSource.toString().length > 0
    readonly property color contentColor: !root.enabled ? Theme.colors.text.disabled
        : (root.chromeSelected || root.hovered || root.visualFocus) ? Theme.colors.text.active
        : root.emphasizedText ? Theme.colors.text.primary : Theme.colors.text.secondary
    stateColors: Theme.colors.listState

    implicitHeight: 30
    height: implicitHeight
    leftPadding: root.hasIcon ? 8 : root.textLeftPadding

    contentItem: RowLayout {
        spacing: 6

        NavigationIcon {
            visible: root.hasIcon
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Layout.alignment: Qt.AlignVCenter
            source: root.iconSource
            filledSource: root.filledIconSource
            selected: root.selected
            hovered: root.hovered || root.visualFocus
            pressed: root.down
            color: root.contentColor
        }

        Text {
            Layout.fillWidth: true
            text: root.text
            color: root.contentColor
            font.family: Theme.uiFont
            font.pixelSize: root.textPixelSize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
