import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

// Multi-line field matching AppTextField chrome (metadata extra fields).
TextArea {
    id: root

    property bool reservesPlainSpace: true
    property color backgroundColor: Theme.colors.background.control
    property bool outlined: false

    ContextMenu.menu: AppTextContextMenu {
        editor: root
    }

    font: Theme.codeFont
    color: Theme.colors.text.editor
    placeholderTextColor: Theme.colors.text.secondary
    selectedTextColor: Theme.colors.text.primary
    selectionColor: Theme.colors.state.textSelection
    leftPadding: 10
    rightPadding: 10
    topPadding: 8
    bottomPadding: 8
    wrapMode: TextEdit.NoWrap
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.controlRadius
        color: Theme.overlayColor(root.enabled
               ? root.backgroundColor
               : Theme.colors.background.controlDisabled)
        border.width: root.enabled && root.activeFocus ? Theme.controlBorderWidth
            : root.outlined ? 1 / root.Screen.devicePixelRatio
            : root.enabled && (root.activeFocus || root.hovered) ? Theme.controlBorderWidth : 0
        border.color: root.enabled && (root.activeFocus || (!root.outlined && root.hovered))
            ? Theme.colors.accent.primary
            : root.enabled && root.hovered ? Theme.colors.border.control : Theme.colors.border.normal
    }
}
