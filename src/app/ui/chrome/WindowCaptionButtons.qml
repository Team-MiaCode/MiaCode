pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Row {
    id: root

    required property var hostWindow
    required property var windowChrome
    readonly property var stateColors: Theme.chromeStateColorsFor(root.hostWindow.nativeMaterialActive)

    height: parent ? parent.height : 34

    CaptionButton {
        buttonType: "minimize"
        accessibleName: qsTrId("qml.minimize")
        onClicked: root.windowChrome.minimize()
    }

    CaptionButton {
        buttonType: root.hostWindow.visibility === Window.Maximized ? "restore" : "maximize"
        // 还原 here means "restore the window", not the 还原 the HUD-font and
        // export pages use for "reset". The Chinese source is the same string,
        // so this one call site passes the key explicitly rather than relying on
        // the shared source-to-key table.
        accessibleName: root.hostWindow.visibility === Window.Maximized
                        ? qsTrId("window.restore") : qsTrId("qml.maximize")
        onClicked: root.windowChrome.toggleMaximized()
    }

    CaptionButton {
        buttonType: "close"
        isClose: true
        accessibleName: qsTrId("action.close")
        onClicked: root.hostWindow.close()
    }

    component CaptionButton: AbstractButton {
        id: button

        required property string buttonType
        required property string accessibleName
        property bool isClose: false

        width: 46
        height: root.height
        padding: 0
        hoverEnabled: true
        Accessible.name: accessibleName

        readonly property color iconColor: button.isClose && (button.hovered || button.down)
            ? "#FFFFFF"
            : (button.hovered || button.visualFocus ? Theme.colors.text.active : Theme.colors.text.secondary)

        contentItem: Item {
            implicitWidth: 14
            implicitHeight: 14

            ControlsImpl.IconImage {
                anchors.centerIn: parent
                width: 14
                height: 14
                source: Qt.resolvedUrl(button.buttonType === "minimize" ? "icons/remove.svg"
                    : button.buttonType === "maximize" ? "icons/maximize.svg"
                    : button.buttonType === "restore" ? "icons/restore.svg" : "icons/close.svg")
                sourceSize: Qt.size(14, 14)
                color: button.iconColor
            }
        }

        background: Rectangle {
            color: button.isClose
                ? (button.down ? "#B32617" : button.hovered ? "#C42B1C" : "transparent")
                : (button.down ? Theme.overlayColor(root.stateColors.pressed)
                               : button.hovered ? Theme.overlayColor(root.stateColors.hover)
                                                : "transparent")
        }
    }
}
