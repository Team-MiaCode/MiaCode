import QtQuick
import MiaCode.UI

// Shared state fill. Controls supply state; geometry and colors stay here.
Item {
    id: root

    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property bool focused: false
    property color baseColor: "transparent"
    property color borderColor: "transparent"
    property real borderWidth: 0
    property real cornerRadius: Theme.controlRadius
    property real highlightOutset: Theme.chromeHighlightOutset
    property var stateColors: Theme.colors.state
    property real contentWidth: width
    property real contentHeight: height
    readonly property real horizontalInset: (width - fill.width) / 2

    readonly property color fillColor: !root.enabled ? root.baseColor
        : root.pressed ? root.stateColors.pressed
        : root.hovered ? root.stateColors.hover
        : (root.selected || root.focused) ? root.stateColors.selected
        : root.baseColor

    Rectangle {
        id: fill
        anchors.centerIn: parent
        width: Math.min(root.width - 2 * Theme.chromeInsetX,
                        Math.max(Theme.chromeMinSize, root.contentWidth + 2 * Theme.chromePadding))
               + 2 * root.highlightOutset
        height: Math.min(root.height - 2 * Theme.chromeInsetY,
                         Math.max(Theme.chromeMinSize, root.contentHeight + 2 * Theme.chromePadding))
                + 2 * root.highlightOutset
        radius: root.cornerRadius
        color: Theme.overlayColor(root.fillColor)
        border.color: root.borderColor
        border.width: root.borderWidth
        border.pixelAligned: false
    }
}
