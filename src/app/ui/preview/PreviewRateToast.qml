import QtQuick
import MiaCode.UI

// Playback-rate changes stay readable as the note viewport shrinks.
// The visual item leaves pointer input to the timeline underneath.
Item {
    id: root

    required property var previewSession
    property rect noteViewportRect: Qt.rect(0, 0, root.width, root.height)
    // Keep the notification visible briefly after the latest rate change.
    property int holdMilliseconds: 900

    readonly property real rate: root.previewSession ? root.previewSession.rate : 1
    property int percent: 100
    property bool showing: false
    // The first evaluation of `rate` is the session's current speed, not a
    // change anyone asked for. Announcing it would flash the HUD every time the
    // pane is built during page switches.
    property bool armed: false

    onPreviewSessionChanged: root.armed = false

    onRateChanged: {
        const next = Math.round(root.rate * 100)
        if (!root.armed) {
            root.armed = true
            root.percent = next
            return
        }
        if (next === root.percent)
            return
        root.percent = next
        root.showing = true
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: root.holdMilliseconds
        onTriggered: root.showing = false
    }

    Item {
        id: card

        readonly property real noteViewportScale: Math.min(1,
            Math.max(0, root.noteViewportRect.width - 16) / width,
            Math.max(0, root.noteViewportRect.height - 16) / height)
        readonly property real panelScale: Math.min(1,
            Math.max(0, root.width - 16) / width,
            Math.max(0, root.height - 16) / height)

        x: {
            const halfWidth = width * scale / 2
            const centerX = root.noteViewportRect.x + root.noteViewportRect.width / 2
            return Math.max(8 + halfWidth,
                Math.min(centerX, root.width - 8 - halfWidth)) - width / 2
        }
        y: {
            const halfHeight = height * scale / 2
            const centerY = root.noteViewportRect.y + root.noteViewportRect.height / 2
            return Math.max(8 + halfHeight,
                Math.min(centerY, root.height - 8 - halfHeight)) - height / 2
        }
        width: Math.max(196, body.implicitWidth + 48)
        height: Math.max(96, body.implicitHeight + 36)
        transformOrigin: Item.Center
        // Preserve the readable size until the whole panel needs to shrink it.
        scale: Math.min(panelScale, Math.max(0.8, noteViewportScale))
        visible: opacity > 0 && scale > 0
            && root.noteViewportRect.width > 0 && root.noteViewportRect.height > 0
        opacity: 0
        enabled: false
        focus: false
        state: root.showing ? "shown" : ""

        states: State {
            name: "shown"
            PropertyChanges {
                target: card
                opacity: 1
            }
        }

        transitions: [
            Transition {
                to: "shown"
                PropertyAction { property: "opacity"; value: 1 }
            },
            FadeTransition {
                from: "shown"
                to: ""
                appearing: false
                initialOpacity: card.opacity
            }
        ]

        FloatingCard {
            anchors.fill: parent
            cornerRadius: Theme.popupRadius
            tintColor: Theme.colors.background.elevated
            opacity: 0.75
        }

        Column {
            id: body
            anchors.centerIn: parent
            anchors.alignWhenCentered: false
            spacing: 6

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.alignWhenCentered: false
                text: qsTrId("timeline.playback_speed")
                color: Theme.colors.text.primary
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize + 1
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.alignWhenCentered: false
                text: root.percent + "%"
                color: Theme.colors.text.heading
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize * 2
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
