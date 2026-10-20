import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

ToolTip {
    id: root

    readonly property Window hostWindow: root.parent ? root.parent.Window.window : null
    popupType: Popup.Item
    z: 1000
    margins: 6
    topMargin: root.hostWindow?.tooltipTopMargin ?? 6
    y: {
        const above = -root.implicitHeight - 6
        const anchorTop = root.parent && root.hostWindow
            ? root.parent.mapToItem(root.Overlay.overlay, 0, 0).y : 0
        return anchorTop + above >= root.topMargin
            ? above : (root.parent ? root.parent.height : 0) + 6
    }

    enter: Transition {
        PropertyAction { property: "opacity"; value: 1 }
    }
    exit: FadeTransition {
        appearing: false
        initialOpacity: root.opacity
    }

    delay: 550
    timeout: 3500
    padding: 6

    contentItem: Text {
        text: root.text
        color: Theme.colors.text.primary
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
    }

    background: FloatingCard {
        popup: root
        cornerRadius: Theme.controlRadius
    }
}
