import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Item {
    id: root

    property string severity: "information"
    property string message: ""
    property bool progressVisible: false
    property real progressPercent: 0
    property bool progressIndeterminate: false
    property string actionText: ""
    property bool actionEnabled: true
    property int holdDuration: Theme.toastHoldDuration
    signal actionTriggered()
    readonly property bool hasErrorIcon: severity === "error"
    readonly property real iconExtent: hasErrorIcon
        ? Theme.toastIconSize + Theme.toastContentSpacing : 0
    readonly property real availableWidth: parent
        ? Math.max(0, parent.width - 2 * Theme.toastHorizontalPadding) : 0

    implicitWidth: progressVisible ? Theme.progressToastWidth
        : label.implicitWidth + iconExtent + 2 * Theme.toastHorizontalPadding
    implicitHeight: progressVisible
        ? Math.max(Theme.progressToastHeight, header.height + percentLabel.implicitHeight
                   + 2 * Theme.toastVerticalPadding + 2 * Theme.toastContentSpacing
                   + Theme.progressToastBarHeight)
        : Math.max(label.implicitHeight, hasErrorIcon ? Theme.toastIconSize : 0)
          + 2 * Theme.toastVerticalPadding
    width: Math.min(implicitWidth, availableWidth)
    height: implicitHeight
    x: parent ? (parent.width - width) / 2 : 0
    y: progressVisible ? Theme.progressToastTopMargin : Theme.toastTopMargin
    z: 10000
    opacity: 0
    visible: opacity > 0 && width > 0
    enabled: progressVisible
    focus: false
    Accessible.role: progressVisible ? Accessible.Grouping : Accessible.StaticText
    Accessible.name: message

    function present(nextSeverity, text, stayOpen = false) {
        hold.stop()
        fade.stop()
        severity = nextSeverity
        message = text
        opacity = 1
        if (!stayOpen)
            hold.restart()
    }

    function dismiss() {
        hold.stop()
        fade.stop()
        opacity = 0
    }

    Timer {
        id: hold
        interval: root.holdDuration
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
        height: root.progressVisible ? root.height - 2 * Theme.toastVerticalPadding
            : Math.max(label.implicitHeight, root.hasErrorIcon ? Theme.toastIconSize : 0)

        Item {
            id: header
            width: parent.width
            y: root.progressVisible ? -Theme.toastContentSpacing / 2 : 0
            height: root.progressVisible
                ? Math.max(Theme.progressToastActionHeight, label.implicitHeight)
                : content.height

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
                width: Math.max(0, header.width - root.iconExtent
                    - (actionButton.visible ? actionButton.width + Theme.toastContentSpacing : 0))
                text: root.message.replace(/[\r\n\u2028\u2029]+/g, " ")
                font.family: Theme.uiFont
                font.pixelSize: root.progressVisible ? Theme.uiFontSize : Theme.toastFontSize
                color: Theme.colors.text.primary
                textFormat: Text.PlainText
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }

            AppButton {
                id: actionButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: Math.max(Theme.progressToastActionMinWidth,
                    contentItem.implicitWidth + leftPadding + rightPadding)
                implicitHeight: Theme.progressToastActionHeight
                width: Math.min(implicitWidth, header.width)
                height: implicitHeight
                leftPadding: Theme.toastContentSpacing
                rightPadding: Theme.toastContentSpacing
                font.pixelSize: Theme.toastFontSize
                visible: root.progressVisible && root.actionText.length > 0
                enabled: root.actionEnabled
                text: root.actionText
                onClicked: root.actionTriggered()
                background: HoverChrome {
                    stateColors: Theme.colors.buttonState
                    hovered: actionButton.hovered
                    pressed: actionButton.down
                    focused: actionButton.visualFocus
                }
            }
        }

        ProgressBar {
            id: progressBar
            anchors.left: parent.left
            anchors.right: parent.right
            y: header.height + Theme.toastContentSpacing / 2
            height: Theme.progressToastBarHeight
            padding: 0
            visible: root.progressVisible
            from: 0
            to: 100
            value: root.progressPercent
            indeterminate: root.progressIndeterminate
            palette.dark: Theme.colors.text.secondary
            background: Rectangle {
                height: progressBar.height
                radius: height / 2
                color: Theme.colors.border.control
            }
        }

        Text {
            id: percentLabel
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: root.progressVisible && !root.progressIndeterminate
            text: Math.round(root.progressPercent) + "%"
            font.family: Theme.uiFont
            font.pixelSize: Theme.toastFontSize
            color: Theme.colors.text.secondary
            textFormat: Text.PlainText
        }
    }
}
