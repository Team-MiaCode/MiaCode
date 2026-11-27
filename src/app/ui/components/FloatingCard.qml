import QtQuick
import QtQuick.Templates as T
import QtQuick.Effects
import QtQuick.Window
import MiaCode.UI

// 卡片填满浮层背景，内容留白由浮层 padding 决定。
// 阴影按圆角矩形绘制，独立于实时背景采样。
Item {
    id: root

    property T.Popup popup: null
    property bool largeSurface: false
    property color tintColor: largeSurface ? Theme.dialogTintColor : Theme.popupTintColor
    property int blurRadius: largeSurface ? Theme.dialogBlurRadius : Theme.popupBlurRadius
    property real blurScale: largeSurface ? Theme.dialogBlurScale : Theme.popupBlurScale
    property real blurMultiplier: largeSurface ? Theme.dialogBlurMultiplier : Theme.popupBlurMultiplier
    property real backdropSaturation: largeSurface ? Theme.dialogBlurSaturation : Theme.popupBlurSaturation
    property real cornerRadius: popup ? Theme.popupRadius : Theme.controlRadius
    property real shadowBlur: largeSurface ? 1.25 : 0.85
    property real shadowOpacity: largeSurface ? Theme.dialogShadowOpacity : Theme.popupShadowOpacity
    property real contactShadowOpacity: largeSurface ? Theme.dialogContactShadowOpacity : Theme.popupContactShadowOpacity
    property real shadowVerticalOffset: largeSurface ? 8 : 5
    property Component tintDelegate: null
    property bool retainBackdrop: false
    property bool liveBackdrop: true
    property bool backdropCreated: false
    readonly property bool materialActive: backdrop.active
    readonly property Item backdropSource: root.Window.window?.backdropSource ?? null

    RectangularShadow {
        anchors.fill: parent
        radius: root.cornerRadius
        blur: 32 * root.shadowBlur
        offset: Qt.vector2d(0, root.shadowVerticalOffset)
        color: Qt.rgba(0, 0, 0, root.shadowOpacity)
        visible: root.shadowOpacity > 0
    }

    RectangularShadow {
        anchors.fill: parent
        radius: root.cornerRadius
        blur: root.largeSurface ? 10 : 8
        offset: Qt.vector2d(0, root.largeSurface ? 2 : 1)
        color: Qt.rgba(0, 0, 0, root.contactShadowOpacity)
        visible: root.shadowOpacity > 0 && root.contactShadowOpacity > 0
    }

    Loader {
        id: backdrop
        anchors.fill: parent
        active: Theme.blurMaterialsEnabled && root.popup !== null
                && (root.popup.visible || (root.retainBackdrop && root.backdropCreated))
                && root.backdropSource !== null && root.width > 0 && root.height > 0
        visible: root.popup !== null && root.popup.visible
        onActiveChanged: if (active) root.backdropCreated = true
        sourceComponent: BackdropBlur {
            sourceItem: root.backdropSource
            popup: root.popup
            blurRadius: root.blurRadius
            blurScale: root.blurScale
            blurMultiplier: root.blurMultiplier
            backdropSaturation: root.backdropSaturation
            cornerRadius: root.cornerRadius
            liveBackdrop: root.liveBackdrop
        }
    }

    Rectangle {
        id: card
        anchors.fill: parent
        visible: root.tintDelegate === null
        radius: root.cornerRadius
        // Translucent tint belongs to the sampled material. A plain floating
        // surface keeps its theme color opaque over both wallpaper and UI.
        color: backdrop.active ? root.tintColor
               : Qt.rgba(root.tintColor.r, root.tintColor.g, root.tintColor.b, 1.0)
    }

    Loader {
        anchors.fill: parent
        active: root.tintDelegate !== null
        sourceComponent: root.tintDelegate
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        color: "transparent"
        border.width: 1
        border.color: Theme.floatingBorderColor
        border.pixelAligned: false
    }
}
