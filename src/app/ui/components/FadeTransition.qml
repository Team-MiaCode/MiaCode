import QtQuick
import MiaCode.UI

Transition {
    id: root

    property bool appearing: true
    property real initialOpacity: appearing ? 0 : 1

    NumberAnimation {
        property: "opacity"
        from: root.initialOpacity
        to: root.appearing ? 1 : 0
        duration: root.appearing ? Theme.popupEnterDuration : Theme.popupExitDuration
        easing.type: root.appearing ? Easing.OutCubic : Easing.InCubic
    }
}
