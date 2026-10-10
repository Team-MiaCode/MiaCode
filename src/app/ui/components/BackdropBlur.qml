import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick.Effects
import QtQuick.Window
import QtQml.Models
import MiaCode.UI

Item {
    id: root

    required property Item sourceItem
    required property T.Popup popup
    property int blurRadius: Theme.popupBlurRadius
    property real cornerRadius: Theme.popupRadius
    readonly property real padding: blurRadius
    readonly property real sampleWidth: width + 2 * padding
    readonly property real sampleHeight: height + 2 * padding
    // A dropdown in a dialog also samples that dialog, which is a sibling of
    // the dropdown in Overlay. The current popup never enters its own source.
    readonly property Item overlaySource: {
        const overlay = root.popup.Overlay.overlay
        let item = root.popup.parent
        while (item && item !== overlay) {
            if (item.parent === overlay)
                return item
            item = item.parent
        }
        return null
    }
    property rect sceneRect
    property rect overlayRect
    // Geometry signals cover popup placement and ancestor movement without
    // running coordinate mapping on every animation frame.
    readonly property var geometryItems: {
        const items = []
        for (const start of [root, root.sourceItem, root.overlaySource]) {
            let item = start
            while (item) {
                if (items.indexOf(item) < 0)
                    items.push(item)
                item = item.parent
            }
        }
        return items
    }

    function queueSourceRectUpdate() {
        sourceRectUpdate.restart()
    }

    function sampleRect(item) {
        const p = root.mapToItem(item, -padding, -padding)
        return Qt.rect(p.x, p.y, sampleWidth, sampleHeight)
    }

    function updateSourceRects() {
        root.sceneRect = root.sampleRect(root.sourceItem)
        if (root.overlaySource)
            root.overlayRect = root.sampleRect(root.overlaySource)
    }

    Timer {
        id: sourceRectUpdate
        interval: 0
        onTriggered: root.updateSourceRects()
    }

    Component.onCompleted: updateSourceRects()
    onGeometryItemsChanged: queueSourceRectUpdate()
    onPaddingChanged: queueSourceRectUpdate()
    onOverlaySourceChanged: queueSourceRectUpdate()
    Instantiator {
        model: root.geometryItems
        delegate: Connections {
            required property var modelData
            target: modelData
            function onXChanged() { root.queueSourceRectUpdate() }
            function onYChanged() { root.queueSourceRectUpdate() }
            function onWidthChanged() { root.queueSourceRectUpdate() }
            function onHeightChanged() { root.queueSourceRectUpdate() }
            function onScaleChanged() { root.queueSourceRectUpdate() }
            function onRotationChanged() { root.queueSourceRectUpdate() }
            function onTransformOriginChanged() { root.queueSourceRectUpdate() }
        }
    }

    Rectangle {
        id: capture
        width: Math.ceil(root.sampleWidth * Theme.popupBlurScale)
        height: Math.ceil(root.sampleHeight * Theme.popupBlurScale)
        visible: false
        color: Theme.colors.background.surface

        ShaderEffectSource {
            id: sceneSample
            anchors.fill: parent
            sourceItem: root.sourceItem
            sourceRect: root.sceneRect
            textureSize: Qt.size(capture.width, capture.height)
        }
        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: root.overlaySource
            sourceRect: root.overlayRect
            textureSize: Qt.size(capture.width, capture.height)
        }
    }

    ShaderEffectSource {
        id: sample
        width: capture.width
        height: capture.height
        sourceItem: root.overlaySource ? capture : null
        textureSize: Qt.size(capture.width, capture.height)
        visible: false
    }

    Item {
        id: mask
        width: root.sampleWidth
        height: root.sampleHeight
        visible: false
        layer.enabled: true
        Rectangle {
            x: root.padding
            y: root.padding
            width: root.width
            height: root.height
            radius: root.cornerRadius
            color: "white"
        }
    }

    // Compositing a blurred premultiplied scene over a constant opaque base
    // preserves the interior color without a separate composition texture.
    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        color: Theme.colors.background.surface
        visible: !root.overlaySource
    }

    MultiEffect {
        x: -root.padding
        y: -root.padding
        width: root.sampleWidth
        height: root.sampleHeight
        // Ordinary popups can blur the scene texture itself. Only a popup
        // inside a dialog needs the extra scene/dialog composition texture.
        source: root.overlaySource ? sample : sceneSample
        autoPaddingEnabled: false
        blurEnabled: true
        blurMax: root.blurRadius * Theme.popupBlurScale
        blur: 1.0
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
