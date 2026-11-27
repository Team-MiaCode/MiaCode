import QtQuick
import MiaCode.UI

// Shared interactive state fill. Animates pure GPU opacity on the latched
// highlight overlay so fading never interpolates through #00000000 and press
// feedback snaps immediately.
Item {
    id: root

    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property bool focused: false
    property bool showFocusRing: true
    property bool emphasizedFill: false
    property bool animateColor: true
    property color baseColor: "transparent"
    property color borderColor: "transparent"
    property real borderWidth: 0
    property real cornerRadius: Theme.controlRadius
    property real highlightOutset: Theme.chromeHighlightOutset
    property bool fillBounds: false
    property bool fillWidth: false
    property var stateColors: Theme.colors.state
    property real contentWidth: width
    property real contentHeight: height
    readonly property real horizontalInset: (width - fill.width) / 2
    readonly property bool focusRingActive: root.enabled && root.focused && root.showFocusRing
    readonly property bool interactiveActive: root.enabled && (root.pressed || root.selected || root.hovered)

    readonly property color activeStateColor: root.pressed ? root.stateColors.pressed
        : root.selected && root.hovered ? (root.stateColors.selectedHover ?? root.stateColors.selected)
        : root.selected ? root.stateColors.selected
        : root.stateColors.hover

    property color latchedStateColor: activeStateColor
    onActiveStateColorChanged: if (interactiveActive) latchedStateColor = activeStateColor
    onInteractiveActiveChanged: if (interactiveActive) latchedStateColor = activeStateColor

    Rectangle {
        id: fill
        anchors.centerIn: parent
        // Keep odd-sized fills centered in baseline units.
        anchors.alignWhenCentered: false
        width: root.fillBounds || root.fillWidth ? root.width
            : Math.min(root.width - 2 * Theme.chromeInsetX,
                       Math.max(Theme.chromeMinSize, root.contentWidth + 2 * Theme.chromePadding))
              + 2 * root.highlightOutset
        height: root.fillBounds ? root.height
            : Math.min(root.height - 2 * Theme.chromeInsetY,
                       Math.max(Theme.chromeMinSize, root.contentHeight + 2 * Theme.chromePadding))
              + 2 * root.highlightOutset
        radius: root.cornerRadius
        color: root.emphasizedFill
            ? Theme.emphasisOverlayColor(root.baseColor)
            : Theme.overlayColor(root.baseColor)
        border.color: root.focusRingActive ? Theme.colors.accent.focus : root.borderColor
        border.width: root.focusRingActive ? Math.max(Theme.controlBorderWidth, root.borderWidth) : root.borderWidth
        border.pixelAligned: false

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: root.emphasizedFill
                ? Theme.emphasisOverlayColor(root.latchedStateColor)
                : Theme.overlayColor(root.latchedStateColor)
            opacity: root.interactiveActive ? 1 : 0
            visible: opacity > 0.005

            Behavior on opacity {
                enabled: Theme.animationsEnabled && root.animateColor && root.visible && !root.pressed
                NumberAnimation { duration: Theme.hoverTransitionDuration; easing.type: Easing.OutCubic }
            }
        }
    }
}
