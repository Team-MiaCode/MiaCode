import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import MiaCode.UI

// Shared combo — geometry mirrors v1 dialogComboBoxStyleSheet (QML Popup, no Win11 chrome).
ComboBox {
    id: root

    property bool compact: false
    // Font pickers render each choice in its own family, including their
    // context-specific default. Ordinary dropdowns keep the UI font.
    property string fontFamilyRole: ""
    property string defaultFontFamily: Theme.uiFont

    function optionFamily(index) {
        if (root.fontFamilyRole && root.model && index >= 0 && index < root.count)
            return root.model[index][root.fontFamilyRole] || root.defaultFontFamily
        return root.defaultFontFamily
    }

    function optionFont(index) {
        return root.fontFamilyRole ? Qt.font({ family: root.optionFamily(index),
                                              pixelSize: root.font.pixelSize,
                                              weight: root.font.weight }) : root.font
    }

    font.family: root.optionFamily(root.currentIndex)
    font.pixelSize: root.compact ? Theme.secondaryFontSize : Theme.uiFontSize
    implicitHeight: Theme.controlMinHeight
    Layout.preferredHeight: implicitHeight
    Layout.maximumHeight: implicitHeight
    leftPadding: 10
    rightPadding: 28
    hoverEnabled: true

    readonly property FontMetrics optionMetrics: FontMetrics {}

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: root.displayText
        font: root.font
        color: root.enabled ? Theme.colors.text.active : Theme.colors.text.disabled
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: ControlsImpl.IconImage {
        x: root.width - width - 8
        y: root.topPadding + (root.availableHeight - height) / 2
        width: 12
        height: 12

        source: Qt.resolvedUrl("icons/chevron-down.svg")
        sourceSize: Qt.size(12, 12)
        color: root.enabled ? Theme.colors.text.secondary : Theme.colors.text.disabled
    }

    background: Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: Theme.chromeInsetY
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.chromeInsetY
        implicitHeight: root.implicitHeight
        radius: Theme.controlRadius
        color: Theme.overlayColor(root.enabled
               ? Theme.colors.background.control
               : Theme.colors.background.controlDisabled)
        border.width: root.enabled && (root.visualFocus || root.hovered || root.down)
                      ? Theme.controlBorderWidth : 0
        border.color: Theme.colors.accent.primary
    }

    delegate: ChromeRow {
        stateColors: Theme.colors.popupState
        id: itemDelegate
        width: ListView.view ? ListView.view.width : root.width
        height: 28
        highlighted: root.highlightedIndex === index
        text: root.textAt(index)
        labelFont: root.optionFont(index)
    }

    popup: AppDropdownPanel {
        property real optionWidth: 0

        y: root.height + 2
        implicitWidth: Math.max(root.width, 100, optionWidth + leftPadding + rightPadding)
        width: Overlay.overlay
            ? Math.min(implicitWidth, Math.max(0, Overlay.overlay.width - 2 * margins))
            : implicitWidth
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 260)
        height: Overlay.overlay
            ? Math.min(implicitHeight, Math.max(0, Overlay.overlay.height - 2 * margins))
            : implicitHeight

        onAboutToShow: {
            let widest = 0
            for (let i = 0; i < root.count; ++i) {
                root.optionMetrics.font = root.optionFont(i)
                widest = Math.max(widest, root.optionMetrics.advanceWidth(root.textAt(i)))
            }
            optionWidth = Math.ceil(widest) + 2 * Theme.rowPaddingX
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.delegateModel
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
