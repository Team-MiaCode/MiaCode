import QtQuick
import QtQuick.Effects
import MiaCode.UI

// Each window owns a complete wallpaper and a base for transparent image pixels.
Item {
    id: root
    property bool nativeMaterialActive: false
    enabled: false
    clip: true

    Rectangle {
        anchors.fill: parent
        visible: Theme.backgroundActive || !root.nativeMaterialActive
        color: Theme.colors.background.surface
    }

    Image {
        anchors.fill: parent
        source: Theme.backgroundActive ? Theme.appBackground.sourceUrl : ""
        visible: Theme.backgroundActive
        opacity: Theme.appBackground ? Theme.appBackground.opacity : 1
        smooth: true
        fillMode: {
            switch (Theme.appBackground ? Theme.appBackground.sizeMode : "cover") {
            case "contain": return Image.PreserveAspectFit
            case "stretch": return Image.Stretch
            case "center": return Image.Pad
            case "repeat": return Image.Tile
            default: return Image.PreserveAspectCrop
            }
        }
        horizontalAlignment: {
            const position = Theme.appBackground ? Theme.appBackground.position : "center"
            return position.indexOf("left") >= 0 ? Image.AlignLeft
                : position.indexOf("right") >= 0 ? Image.AlignRight : Image.AlignHCenter
        }
        verticalAlignment: {
            const position = Theme.appBackground ? Theme.appBackground.position : "center"
            return position.indexOf("top") >= 0 ? Image.AlignTop
                : position.indexOf("bottom") >= 0 ? Image.AlignBottom : Image.AlignVCenter
        }
        layer.enabled: Theme.backgroundActive && Theme.blurMaterialsEnabled
            && Theme.appBackground.blur > 0
        layer.effect: MultiEffect {
            blurEnabled: true
            blurMax: 64
            blur: Theme.appBackground ? Math.min(1, Theme.appBackground.blur / 64) : 0
        }
    }
}
