import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import MiaCode.UI

Item {
    id: root

    signal released()
    property bool showDivider: true

    implicitWidth: Theme.splitDividerThickness
    implicitHeight: Theme.splitDividerThickness
    // Paint above both panes so the thickened stroke is visible on each side.
    z: 1
    property bool handlePressed: Controls.SplitHandle.pressed
    property bool handleHovered: Controls.SplitHandle.hovered
    readonly property bool handleActive: handlePressed || handleHovered
    readonly property bool verticalDivider: parent !== null
                                            && parent.orientation === Qt.Horizontal
    readonly property real lineThickness: handleActive
                                          ? Theme.splitHandleActiveThickness
                                          : 1 / root.Screen.devicePixelRatio

    containmentMask: Item {
        x: root.verticalDivider ? (root.width - width) / 2 : 0
        y: root.verticalDivider ? 0 : (root.height - height) / 2
        width: root.verticalDivider ? Theme.splitHandleHitExtent : root.width
        height: root.verticalDivider ? root.height : Theme.splitHandleHitExtent
    }

    // HoverHandler 只设置光标，不拦截指针事件，不干扰 SplitView 的拖拽。
    HoverHandler {
        cursorShape: root.verticalDivider ? Qt.SplitHCursor : Qt.SplitVCursor
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceColor(Theme.colors.background.panel)
    }

    Rectangle {
        x: root.verticalDivider
            ? Math.floor((parent.width - width) * root.Screen.devicePixelRatio / 2) / root.Screen.devicePixelRatio : 0
        y: root.verticalDivider ? 0
            : Math.floor((parent.height - height) * root.Screen.devicePixelRatio / 2) / root.Screen.devicePixelRatio
        visible: root.showDivider || root.handleActive
        width: root.verticalDivider ? root.lineThickness : parent.width
        height: root.verticalDivider ? parent.height : root.lineThickness
        color: root.handleActive
               ? Theme.colors.accent.focus
               : Theme.separatorColor
    }

    property bool wasPressed: false

    onVisibleChanged: wasPressed = false

    onHandlePressedChanged: {
        if (root.wasPressed && !root.handlePressed)
            root.released()
        root.wasPressed = root.handlePressed
    }
}
