import QtQuick
import QtQuick.Controls
import MiaCode.UI

// Shared slider. A thin neutral rail and fill leave the handle as the brightest
// part; only the export-range highlight keeps the accent.
// Control 会把 background 拉到整颗滑条的尺寸，轨道必须画在内层，不能写在
// background 根上，否则轨道会变成整块色条。
Slider {
    id: root

    property bool rangeMarkersVisible: false
    property bool rangeHighlightVisible: false
    property real rangeStartValue: 0
    property real rangeEndValue: 0

    function positionForValue(value) {
        if (root.to <= root.from)
            return root.handle.width / 2
        const fraction = Math.max(0, Math.min(1,
            (value - root.from) / (root.to - root.from)))
        return root.handle.width / 2
            + fraction * Math.max(0, root.availableWidth - root.handle.width)
    }

    function rangeEdgeX(value) {
        if (value <= root.from)
            return 0
        if (value >= root.to)
            return track.width
        return positionForValue(value)
    }

    hoverEnabled: true
    implicitHeight: Theme.controlMinHeight
    padding: 0

    background: Item {
        implicitWidth: 200
        implicitHeight: Theme.controlMinHeight

        Rectangle {
            id: track
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 4
            radius: 2
            color: Theme.overlayColor(Theme.colors.slider.rail)

            Rectangle {
                visible: !root.rangeHighlightVisible
                width: root.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: Theme.colors.slider.fill
            }

            Rectangle {
                visible: root.rangeHighlightVisible && root.to > root.from
                x: root.rangeEdgeX(root.rangeStartValue)
                width: Math.max(0, root.rangeEdgeX(root.rangeEndValue) - x)
                height: parent.height
                radius: parent.radius
                color: Theme.colors.state.followHighlight
            }

            Rectangle {
                visible: root.rangeHighlightVisible && root.to > root.from
                x: root.rangeEdgeX(root.rangeStartValue)
                width: Math.max(0, root.rangeEdgeX(Math.max(root.rangeStartValue,
                    Math.min(root.value, root.rangeEndValue))) - x)
                height: parent.height
                radius: parent.radius
                color: Theme.colors.accent.primary
            }
        }

        Rectangle {
            visible: root.rangeMarkersVisible && root.to > root.from
            x: root.positionForValue(root.rangeStartValue) - width / 2
            y: track.y - height
            width: 2
            height: 4
            radius: 1
            color: Theme.colors.accent.primary
        }

        Rectangle {
            visible: root.rangeMarkersVisible && root.to > root.from
            x: root.positionForValue(root.rangeEndValue) - width / 2
            y: track.y - height
            width: 2
            height: 4
            radius: 1
            color: Theme.colors.accent.primary
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: 12
        implicitHeight: 12
        width: 12
        height: 12
        radius: 6
        color: Theme.colors.slider.handle
        border.width: Theme.controlBorderWidth
        border.color: Theme.colors.toggle.knobBorder

        // Hover/drag halo. A negative z keeps it under the handle itself.
        Rectangle {
            z: -1
            anchors.centerIn: parent
            width: parent.width + 8
            height: width
            radius: width / 2
            visible: root.enabled && (root.hovered || root.pressed)
            color: {
                const c = Qt.color(Theme.colors.text.active)
                return Qt.rgba(c.r, c.g, c.b, root.pressed ? 0.20 : 0.12)
            }
        }
    }
}
