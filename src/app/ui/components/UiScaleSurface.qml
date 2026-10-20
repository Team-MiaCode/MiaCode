import QtQuick
import QtQuick.Controls
import MiaCode.UI

// Keep layout in baseline UI units and scale the entire scene, including
// controls that use fixed font/icon sizes. Popups live outside this tree.
Item {
    id: root

    property real scaleFactor: Theme.uiScale
    readonly property var hostWindow: root.Window.window
    readonly property Item overlayItem: root.Overlay.overlay

    width: parent ? parent.width / scaleFactor : 0
    height: parent ? parent.height / scaleFactor : 0
    scale: scaleFactor
    transformOrigin: Item.TopLeft

    Binding {
        target: root.overlayItem
        property: "scale"
        value: root.scaleFactor
    }
    Binding {
        target: root.overlayItem
        property: "transformOrigin"
        value: Item.TopLeft
    }
    Binding {
        target: root.overlayItem
        property: "width"
        value: root.hostWindow ? root.hostWindow.width / root.scaleFactor : 0
    }
    Binding {
        target: root.overlayItem
        property: "height"
        value: root.hostWindow ? root.hostWindow.height / root.scaleFactor : 0
    }
}
