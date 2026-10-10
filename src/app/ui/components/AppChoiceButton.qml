import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

ToolButton {
    id: root

    property int difficultyId: 0

    checkable: true
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: Theme.controlMinHeight
    padding: 0
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    font.family: Theme.uiFont
    font.pixelSize: Theme.uiFontSize

    contentItem: RowLayout {
        spacing: 6
        DifficultySwatch {
            difficultyId: root.difficultyId
            visible: root.difficultyId > 0
            Layout.alignment: Qt.AlignVCenter
        }
        Text {
            id: label
            text: root.text
            font: root.font
            color: root.checked ? Theme.colors.text.active : Theme.colors.text.secondary
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: HoverChrome {
        highlightOutset: 0
        stateColors: Theme.colors.popupState
        contentHeight: Theme.controlHighlightHeight - 2 * Theme.chromePadding
        selected: root.checked
        hovered: root.hovered
        pressed: root.down
        focused: root.visualFocus
    }
}
