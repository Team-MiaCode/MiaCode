import QtQuick
import QtQuick.Window
import QtQuick.Controls
import MiaCode.UI

MenuSeparator {
    id: root

    topPadding: 7
    bottomPadding: 7
    leftPadding: 6
    rightPadding: 6

    contentItem: Rectangle {
        implicitHeight: 1 / Screen.devicePixelRatio
        color: Theme.separatorColor
    }
}
