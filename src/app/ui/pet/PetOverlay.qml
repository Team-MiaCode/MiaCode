import QtQuick
import QtQuick.Window
import QtQuick.Controls
import MiaCode.UI

Window {
    id: petWindow

    required property var controller
    required property Window hostWindow

    property bool placementApplied: false
    property string animationState: "idle"
    property real animationPhase: 0
    property real lifeTime: 0

    width: controller.size
    height: controller.size
    visible: controller.visible && placementApplied
    color: "transparent"
    title: qsTrId("pet.window_title")
    transientParent: null
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
           | Qt.NoDropShadowWindowHint

    function applyInitialPosition() {
        controller.prepareWindow(petWindow)
        const point = controller.initialPosition(
            width, height,
            hostWindow.x, hostWindow.y, hostWindow.width, hostWindow.height)
        x = point.x
        y = point.y
        placementApplied = true
    }

    function resizePet(size) {
        const previous = controller.size
        controller.size = size
        const delta = controller.size - previous
        x -= delta / 2
        y -= delta
        correctPosition(true)
    }

    function correctPosition(save) {
        const point = controller.correctedPosition(x, y, width, height)
        x = point.x
        y = point.y
        if (save)
            controller.savePosition(x, y, width, height)
    }

    function enterState(nextState) {
        animationState = nextState
        animationPhase = 0
        stateTimer.stop()
        if (nextState === "sleeping")
            return
        stateTimer.interval = nextState === "interaction" ? 2000
                             : nextState === "sitting" ? 12000 : 8000
        stateTimer.restart()
    }

    function advancePoseCycle() {
        enterState(animationState === "interaction" ? "idle"
                   : animationState === "idle" ? "sitting" : "sleeping")
    }

    function interact() {
        enterState("interaction")
    }

    Component.onCompleted: applyInitialPosition()

    onVisibleChanged: {
        if (!visible) {
            stateTimer.stop()
            return
        }
        correctPosition(false)
        enterState("idle")
    }

    onClosing: function(close) {
        close.accepted = false
        controller.visible = false
    }

    Screen.onVirtualXChanged: correctPosition(true)
    Screen.onVirtualYChanged: correctPosition(true)
    Screen.onDesktopAvailableWidthChanged: correctPosition(true)
    Screen.onDesktopAvailableHeightChanged: correctPosition(true)
    Screen.onDevicePixelRatioChanged: correctPosition(true)

    Item {
        id: spriteHost
        anchors.fill: parent

        Accessible.role: Accessible.Animation
        Accessible.name: qsTrId("pet.accessible_name")

        PetScene {
            id: petScene
            anchors.fill: parent
            cameraMirrored: petWindow.controller.cameraMirrored

            model: Fox {
                clip: petWindow.animationState === "sleeping"
                      ? "animation.fox.sleep"
                      : petWindow.animationState === "sitting"
                        ? "animation.fox.sit"
                        : petWindow.animationState === "interaction"
                          ? "animation.fox.wiggle"
                          : ""
                phase: petWindow.animationPhase
                lifeTime: petWindow.lifeTime
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property bool dragging: false
            property point pressPosition
            property point windowPosition

            onPressed: function(mouse) {
                dragging = false
                const point = petWindow.controller.pointerPosition()
                pressPosition = Qt.point(point.x, point.y)
                windowPosition = Qt.point(petWindow.x, petWindow.y)
            }
            onPositionChanged: function(mouse) {
                if (!(pressedButtons & Qt.LeftButton))
                    return
                const point = petWindow.controller.pointerPosition()
                const dx = point.x - pressPosition.x
                const dy = point.y - pressPosition.y
                if (!dragging && Math.hypot(dx, dy) >= Qt.styleHints.startDragDistance)
                    dragging = true
                if (dragging) {
                    petWindow.x = windowPosition.x + dx
                    petWindow.y = windowPosition.y + dy
                }
            }
            onReleased: function(mouse) {
                if (dragging)
                    petWindow.correctPosition(true)
                else if (mouse.button === Qt.LeftButton)
                    petWindow.interact()
                else if (mouse.button === Qt.RightButton)
                    contextMenu.popup(mouse.x, mouse.y)
                dragging = false
            }
            onCanceled: {
                if (dragging)
                    petWindow.correctPosition(true)
                dragging = false
            }
            onWheel: function(wheel) {
                if (wheel.angleDelta.y !== 0)
                    petWindow.resizePet(petWindow.controller.size + (wheel.angleDelta.y > 0 ? 20 : -20))
                wheel.accepted = true
            }
        }

        Menu {
            id: contextMenu
            popupType: Popup.Native
            MenuItem {
                text: qsTrId("pet.camera_mirror")
                checkable: true
                checked: petWindow.controller.cameraMirrored
                onTriggered: petWindow.controller.cameraMirrored = !petWindow.controller.cameraMirrored
            }
            MenuItem {
                text: qsTrId("pet.enlarge")
                enabled: petWindow.controller.size < 440
                onTriggered: petWindow.resizePet(petWindow.controller.size + 20)
            }
            MenuItem {
                text: qsTrId("pet.shrink")
                enabled: petWindow.controller.size > 140
                onTriggered: petWindow.resizePet(petWindow.controller.size - 20)
            }
            MenuItem {
                text: qsTrId("pet.reset_size")
                onTriggered: petWindow.resizePet(220)
            }
            MenuSeparator { }
            MenuItem {
                text: qsTrId("pet.hide")
                onTriggered: petWindow.controller.visible = false
            }
        }

    }

    FrameAnimation {
        running: petWindow.visible
        onTriggered: {
            petWindow.animationPhase += frameTime
            petWindow.lifeTime += frameTime
            petScene.advanceFrame()
        }
    }

    Timer {
        id: stateTimer
        repeat: false
        onTriggered: petWindow.advancePoseCycle()
    }
}
