import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import MiaCode.UI

// Shared menu row — gray HoverChrome; text active / secondary / disabled.
MenuItem {
    id: root

    property bool compact: false

    implicitHeight: compact ? Theme.compactControlHeight : Theme.menuRowHeight
    // Reserve the same 14 px icon slot for submenu arrows and checkmarks.
    leftPadding: subMenu && mirrored ? 40 : 12
    rightPadding: subMenu && !mirrored ? 40 : 16
    topPadding: 3
    bottomPadding: 3
    font.family: Theme.uiFont
    font.pixelSize: compact ? Theme.compactFontSize : Theme.uiFontSize
    // Where the full identity goes when the label is deliberately short — the
    // recent-charts and backup lists show a folder name or a timestamp, and the
    // path they stand for is too wide to be a menu row.
    property string tooltip: ""
    // Dynamic menus own their rows directly, rather than wrapping them in an
    // Action. Keep their shortcut spelling on the visual item so Qt can both
    // insert it into Menu and render the binding.
    property string shortcutText: ""
    property int difficultyId: 0
    implicitWidth: Math.ceil(labelMetrics.advanceWidth + chromeWidth
                             + (shortcutLabel.text.length > 0 ? shortcutLabel.implicitWidth + row.spacing : 0))
    readonly property real chromeWidth: leftPadding + rightPadding
                                        + (checkable ? 14 + row.spacing : 0)
                                        + (difficultyId > 0 ? Theme.difficultySwatchSize + row.spacing : 0)

    TextMetrics {
        id: labelMetrics
        font: root.font
        text: root.text
    }

    readonly property color labelColor: {
        if (!root.enabled)
            return Theme.colors.text.disabled
        if (root.highlighted || root.checked)
            return Theme.colors.text.active
        return Theme.colors.text.secondary
    }

    Tooltip {
        visible: root.tooltip.length > 0 && root.hovered
        text: root.tooltip
    }

    contentItem: RowLayout {
        id: row
        spacing: 10

        DifficultySwatch {
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
            Layout.alignment: Qt.AlignVCenter
            visible: root.difficultyId > 0
            difficultyId: root.difficultyId
        }

        ControlsImpl.MnemonicLabel {
            id: label
            Layout.fillWidth: true
            Layout.preferredWidth: implicitWidth
            text: root.text
            font: root.font
            color: root.labelColor
            mnemonicVisible: true
            elide: Text.ElideRight
        }

        Text {
            id: shortcutLabel
            Layout.minimumWidth: implicitWidth
            // Static rows receive their spelling from an Action; dynamic rows
            // provide shortcutText directly because Repeater must create a
            // visual MenuItem, not a non-visual Action.
            text:
                root.shortcutText.length > 0
                    ? root.shortcutText
                    : (root.action && root.action.shortcutText ? root.action.shortcutText : "")
            visible: text.length > 0
            font: root.font
            color: Theme.colors.text.disabled
            opacity: root.enabled ? 1 : 0.55
        }

        Item {
            Layout.preferredWidth: 14
            visible: root.checkable
        }
    }

    background: HoverChrome {
        cornerRadius: root.compact ? Theme.compactControlRadius : Theme.controlRadius
        stateColors: Theme.colors.popupState
        selected: root.checked
        hovered: root.highlighted || root.hovered
        pressed: root.down
        focused: root.visualFocus
    }

    indicator: ControlsImpl.IconImage {
        x: root.width - width - root.rightPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        width: 14
        height: 14
        visible: root.checkable && root.checked
        source: Qt.resolvedUrl("icons/check.svg")
        sourceSize: Qt.size(14, 14)
        color: root.enabled ? Theme.colors.text.active : Theme.colors.text.disabled
    }

    // Match the checkmark's size and distance from the menu edge.
    arrow: ControlsImpl.IconImage {
        x: root.mirrored ? 16 : root.width - width - 16
        y: root.topPadding + (root.availableHeight - height) / 2
        width: 14
        height: 14
        visible: root.subMenu
        mirror: root.mirrored
        source: Qt.resolvedUrl("icons/chevron-right.svg")
        sourceSize: Qt.size(14, 14)
        color: root.enabled ? Theme.colors.text.active : Theme.colors.text.disabled
    }
}
