import QtQuick
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Item {
    id: root

    property string severity: "information"
    property string message: ""
    readonly property bool hasErrorIcon: severity === "error"
    readonly property real iconExtent: hasErrorIcon
        ? Theme.toastIconSize + Theme.toastContentSpacing : 0
    readonly property real availableWidth: parent
        ? Math.max(0, parent.width - 2 * Theme.toastHorizontalPadding) : 0

    implicitWidth: label.implicitWidth + iconExtent + 2 * Theme.toastHorizontalPadding
    implicitHeight: Math.max(label.implicitHeight, hasErrorIcon ? Theme.toastIconSize : 0)
                    + 2 * Theme.toastVerticalPadding
    width: Math.min(implicitWidth, availableWidth)
    height: implicitHeight
    x: parent ? (parent.width - width) / 2 : 0
    y: Theme.toastTopMargin
    z: 10000
    opacity: 0
    visible: opacity > 0 && width > 0
    enabled: false
    focus: false
    Accessible.role: Accessible.StaticText
    Accessible.name: message

    function present(nextSeverity, text) {
        fade.stop()
        severity = nextSeverity
        message = text
        opacity = 1
        hold.restart()
    }

    Timer {
        id: hold
        interval: Theme.toastHoldDuration
        onTriggered: fade.restart()
    }

    NumberAnimation {
        id: fade
        target: root
        property: "opacity"
        to: 0
        duration: Theme.toastFadeDuration
        easing.type: Easing.InCubic
    }

    FloatingCard {
        anchors.fill: parent
        cornerRadius: Theme.toastRadius
        tintColor: Theme.colors.background.elevated
        opacity: Theme.toastSurfaceOpacity
    }

    Item {
        id: content
        anchors.centerIn: parent
        anchors.alignWhenCentered: false
        width: Math.max(0, root.width - 2 * Theme.toastHorizontalPadding)
        height: Math.max(label.implicitHeight, root.hasErrorIcon ? Theme.toastIconSize : 0)

        ControlsImpl.IconImage {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.toastIconSize
            height: Theme.toastIconSize
            visible: root.hasErrorIcon
            source: Qt.resolvedUrl("icons/toast-error.svg")
            sourceSize: Theme.iconSourceSize(width, height)
            color: Theme.colors.danger.primary
        }

        Text {
            id: label
            x: root.iconExtent
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, content.width - root.iconExtent)
            text: root.message.replace(/[\r\n\u2028\u2029]+/g, " ")
            font.family: Theme.uiFont
            font.pixelSize: Theme.toastFontSize
            color: Theme.colors.text.primary
            textFormat: Text.PlainText
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
        }
    }
}
