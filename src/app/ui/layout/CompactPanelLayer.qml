import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

Item {
    id: root

    required property var viewState
    required property var documentSession
    required property var preferences
    required property var commands
    required property var pages
    property bool compact: false
    property bool peekEnabled: false
    property bool canDock: false
    property bool libraryPopupActive: false
    property bool pendingMediaToolsMenu: false
    property bool peekOpen: false
    property bool awaitingPointerEntry: false
    property bool suppressExitAnimation: false
    readonly property bool drawerActive: sidebarDrawer.visible || sidebarDrawer.position > 0.001
    readonly property bool windowActive: root.Window.window ? root.Window.window.active : false
    readonly property Item activeFocusItem: root.Window.window ? root.Window.window.activeFocusItem : null
    readonly property bool hasActiveDrawerTextInput: {
        const item = root.activeFocusItem
        if (!item || !("cursorPosition" in item && "readOnly" in item && !item.readOnly))
            return false
        let cursor = item
        while (cursor) {
            if (cursor === drawerContent)
                return true
            cursor = cursor.parent
        }
        return false
    }
    readonly property bool hasBlockingPopup: compactSidebar.popupActive
        || root.libraryPopupActive
        || root.hasActiveDrawerTextInput
    readonly property bool peekHoldOpen: drawerHover.hovered
        || drawerPress.active
        || edgePeekHover.hovered
        || (root.canDock && (peekExpandPill.hovered || peekExpandPill.pressed))
        || root.awaitingPointerEntry
        || root.hasBlockingPopup
    readonly property alias libraryHost: compactSidebar.libraryHost

    signal settingsRequested()
    signal mediaToolRequested(string toolId)
    signal dockRequested()

    function openPeek(waitForPointerEntry = false) {
        if (!root.peekEnabled)
            return
        peekOpenTimer.stop()
        peekCloseTimer.stop()
        root.awaitingPointerEntry = waitForPointerEntry
        root.peekOpen = true
    }

    function dismissPeek(immediate = false) {
        peekOpenTimer.stop()
        peekCloseTimer.stop()
        root.awaitingPointerEntry = false
        if (!root.peekOpen)
            return
        if (immediate) {
            root.suppressExitAnimation = true
            root.peekOpen = false
            Qt.callLater(() => { root.suppressExitAnimation = false })
        } else {
            root.peekOpen = false
        }
    }

    function handleWorkspacePress() {
        if (!root.peekOpen || root.hasBlockingPopup)
            return
        if (drawerHover.hovered
                || (root.canDock && (peekExpandPill.hovered || peekExpandPill.pressed)))
            return
        root.dismissPeek(false)
    }

    function toggleCompactDrawer() {
        if (!root.compact)
            return
        if (root.peekOpen)
            root.dismissPeek(false)
        else
            root.openPeek(true)
    }

    function dockFromPeek() {
        root.dismissPeek(true)
        root.dockRequested()
    }

    function showMediaToolsMenu() {
        if (sidebarDrawer.opened) {
            compactSidebar.showMediaToolsMenu()
            return
        }
        root.pendingMediaToolsMenu = true
        root.openPeek(true)
    }

    onPeekOpenChanged: {
        if (root.compact)
            root.viewState.compactPanel = root.peekOpen ? "sidebar" : ""
    }

    onCompactChanged: {
        root.dismissPeek(true)
        if (!compact && root.viewState.compactPanel === "sidebar")
            root.viewState.compactPanel = ""
    }

    onPeekEnabledChanged: {
        if (!peekEnabled)
            root.dismissPeek(true)
    }

    onWindowActiveChanged: {
        if (!windowActive) {
            peekOpenTimer.stop()
            if (root.peekOpen && !root.hasBlockingPopup)
                root.dismissPeek(false)
        }
    }

    Connections {
        target: root.viewState
        function onCompactPanelChanged() {
            if (!root.compact)
                return
            if (root.viewState.compactPanel === "sidebar" && !root.peekOpen)
                root.openPeek(true)
            else if (root.viewState.compactPanel !== "sidebar" && root.peekOpen)
                root.dismissPeek(false)
        }
    }

    z: 40

    // Passive left-edge hover trigger used in both narrow compact mode and
    // collapsed efficient mode.
    Item {
        id: edgePeekZone
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 18
        visible: root.peekEnabled

        HoverHandler {
            id: edgePeekHover
            blocking: false
            onHoveredChanged: {
                if (hovered && root.peekEnabled && !root.peekOpen
                        && root.windowActive && sidebarDrawer.position > 0.01) {
                    root.openPeek(false)
                }
            }
        }
    }

    Timer {
        id: peekOpenTimer
        interval: 55
        running: root.peekEnabled && !root.peekOpen && edgePeekHover.hovered && root.windowActive
        onTriggered: {
            if (root.peekEnabled && edgePeekHover.hovered && root.windowActive)
                root.openPeek(false)
        }
    }

    Timer {
        id: peekCloseTimer
        interval: 380
        running: root.peekOpen && !root.peekHoldOpen
        onTriggered: {
            if (root.peekOpen && !root.peekHoldOpen)
                root.peekOpen = false
        }
    }

    Drawer {
        id: sidebarDrawer
        parent: Overlay.overlay
        edge: Qt.LeftEdge
        width: {
            const preferred = root.preferences.sidebarWidth
                + (root.compact && !root.preferences.efficientSidebarEnabled ? Theme.activityButtonSize : 0)
            const maxWidth = Math.max(root.preferences.sidebarMinimumContentWidth, root.width - 36)
            return Math.max(root.preferences.sidebarMinimumContentWidth, Math.min(preferred, maxWidth))
        }
        height: root.height
        y: {
            root.parent.y
            root.height
            return root.mapToItem(Overlay.overlay, 0, 0).y
        }
        padding: 0
        modal: false
        dim: false
        focus: false
        closePolicy: Popup.CloseOnEscape
        interactive: false
        visible: root.peekEnabled && root.peekOpen

        enter: Transition {
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation {
                duration: root.suppressExitAnimation ? 0 : 160
                easing.type: Easing.OutCubic
            }
        }

        onOpened: {
            if (root.pendingMediaToolsMenu) {
                root.pendingMediaToolsMenu = false
                compactSidebar.showMediaToolsMenu()
            }
        }
        onClosed: {
            root.pendingMediaToolsMenu = false
            root.awaitingPointerEntry = false
            if (root.peekOpen)
                root.peekOpen = false
        }

        // 浮层置于 Overlay，背景采样包含工作区，抽屉自身保持在采样源之外。
        background: FloatingCard {
            popup: sidebarDrawer
            largeSurface: true
            shadowVerticalOffset: 0
        }

        contentItem: Item {
            id: drawerContent

            // Absorb clicks and wheel events on empty or disabled drawer areas
            // so a non-modal floating drawer never leaks input to the workspace below.
            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.AllButtons
                hoverEnabled: false
                onPressed: mouse => mouse.accepted = true
                onWheel: wheel => wheel.accepted = true
            }

            HoverHandler {
                id: drawerHover
                onHoveredChanged: {
                    if (hovered && root.awaitingPointerEntry)
                        root.awaitingPointerEntry = false
                }
            }

            PointHandler {
                id: drawerPress
                acceptedButtons: Qt.AllButtons
            }

            Sidebar {
                id: compactSidebar
                anchors.fill: parent
                viewState: root.viewState
                documentSession: root.documentSession
                preferences: root.preferences
                commands: root.commands
                pages: root.pages
                compact: true
                onSettingsRequested: root.settingsRequested()
                onMediaToolRequested: toolId => root.mediaToolRequested(toolId)
            }
        }
    }

    // Floating dock/expand pill shown only when docking the sidebar is possible
    // (collapsed efficient mode at desktop width, never in narrow compact mode).
    Item {
        id: peekExpandPillHost
        parent: Overlay.overlay
        z: sidebarDrawer.z + 1
        width: peekExpandPill.implicitWidth
        height: peekExpandPill.implicitHeight
        x: sidebarDrawer.position * sidebarDrawer.width - width / 2
        y: sidebarDrawer.y + Math.round((sidebarDrawer.height - height) / 2)
        visible: root.canDock && !root.suppressExitAnimation && sidebarDrawer.position > 0.01
        opacity: sidebarDrawer.position

        SidebarEdgePill {
            id: peekExpandPill
            anchors.fill: parent
            buttonObjectName: "efficientPeekExpandButton"
            iconSource: Qt.resolvedUrl("icons/chevron-right.svg")
            tooltip: qsTrId("qml.open_sidebar")
            interactive: root.canDock && root.peekOpen && sidebarDrawer.position > 0.5
            onClicked: root.dockFromPeek()
        }
    }
}
