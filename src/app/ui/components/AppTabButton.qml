import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

TabButton {
    id: root

    property bool compact: false
    property Component accessory: null

    width: implicitWidth
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: compact ? Theme.compactControlHeight : Theme.controlMinHeight
    padding: 0
    leftPadding: compact ? Theme.compactTabContentPadding : 12
    rightPadding: leftPadding
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    font.family: Theme.uiFont
    font.pixelSize: compact ? Theme.compactFontSize : Theme.uiFontSize
    font.preferTypoLineMetrics: true

    contentItem: RowLayout {
        spacing: 6

        Text {
            id: label
            text: root.text
            font: root.font
            color: root.checked ? Theme.colors.text.active : Theme.colors.text.secondary
            verticalAlignment: Text.AlignVCenter
        }

        Loader {
            sourceComponent: root.accessory
            visible: active && sourceComponent !== null
            Layout.alignment: Qt.AlignVCenter
        }
    }

    background: HoverChrome {
        cornerRadius: root.compact ? Theme.compactControlRadius : Theme.controlRadius
        highlightOutset: 0
        stateColors: Theme.colors.popupState
        contentHeight: Theme.controlHighlightHeight - 2 * Theme.chromePadding
        selected: root.checked
        hovered: root.hovered
        pressed: root.down
        focused: root.visualFocus
    }
}
