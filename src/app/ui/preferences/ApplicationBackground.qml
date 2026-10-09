import QtQuick
import QtQuick.Effects
import MiaCode.UI

// The application wallpaper owns no chart media. Paint it before workspace
// surfaces, whose opacity uses the same Theme.backgroundActive gate.
Item {
    id: root
    required property var appBackground
    enabled: false
    Image {
        objectName: "applicationBackgroundImage"
        anchors.fill: parent
        source: root.appBackground.sourceUrl
        visible: Theme.backgroundActive
        opacity: root.appBackground.opacity
        asynchronous: false
        smooth: true
        fillMode: {
            switch (root.appBackground.sizeMode) {
            case "contain": return Image.PreserveAspectFit
            case "stretch": return Image.Stretch
            case "center": return Image.Pad
            case "repeat": return Image.Tile
            default: return Image.PreserveAspectCrop
            }
        }
        horizontalAlignment: {
            const value = root.appBackground.position
            return value.indexOf("left") >= 0 ? Image.AlignLeft
                 : value.indexOf("right") >= 0 ? Image.AlignRight : Image.AlignHCenter
        }
        verticalAlignment: {
            const value = root.appBackground.position
            return value.indexOf("top") >= 0 ? Image.AlignTop
                 : value.indexOf("bottom") >= 0 ? Image.AlignBottom : Image.AlignVCenter
        }
        layer.enabled: root.appBackground.blur > 0
        layer.effect: MultiEffect {
            blurEnabled: true
            blurMax: 64
            blur: Math.min(1.0, root.appBackground.blur / 64.0)
        }
    }
}
