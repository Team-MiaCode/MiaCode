import QtQuick
import QtQuick.Layouts
import MiaCode.UI

StackLayout {
    id: root

    implicitHeight: currentIndex >= 0 && currentIndex < children.length
                    ? children[currentIndex].implicitHeight : 0

    onCurrentIndexChanged: {
        if (Theme.animationsEnabled && visible)
            pageFade.restart()
        else {
            pageFade.stop()
            opacity = 1
        }
    }

    NumberAnimation {
        id: pageFade
        target: root
        property: "opacity"
        from: 0.45
        to: 1.0
        duration: Theme.stateTransitionDuration
        easing.type: Easing.OutCubic
    }
}
