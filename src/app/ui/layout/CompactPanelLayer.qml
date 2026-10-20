import QtQuick
import QtQuick.Controls
import MiaCode.UI

Item {
    id: root

    required property var viewState
    required property var documentSession
    required property var preferences
    required property var commands
    required property var pages
    property bool compact: false
    property bool pendingMediaToolsMenu: false
    signal settingsRequested()
    signal mediaToolRequested(string toolId)

    function showMediaToolsMenu() {
        if (sidebarDrawer.opened) {
            compactSidebar.showMediaToolsMenu()
            return
        }
        root.pendingMediaToolsMenu = true
        root.viewState.compactPanel = "sidebar"
    }

    z: 40

    Drawer {
        id: sidebarDrawer
        parent: Overlay.overlay
        edge: Qt.LeftEdge
        width: Math.min(320, root.width - 36)
        height: root.height
        y: {
            root.parent.y
            root.height
            return root.mapToItem(Overlay.overlay, 0, 0).y
        }
        padding: 0
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        interactive: root.compact
        visible: root.compact && root.viewState.compactPanel === "sidebar"
        onOpened: {
            root.viewState.compactPanel = "sidebar"
            if (root.pendingMediaToolsMenu) {
                root.pendingMediaToolsMenu = false
                compactSidebar.showMediaToolsMenu()
            }
        }
        onClosed: {
            root.pendingMediaToolsMenu = false
            if (root.viewState.compactPanel === "sidebar")
                root.viewState.compactPanel = ""
        }

        Overlay.modal: Rectangle {
            color: Theme.modalScrimColor
        }

        // 浮层置于 Overlay，背景采样包含工作区，抽屉自身保持在采样源之外。
        background: FloatingCard {
            popup: sidebarDrawer
            tintColor: Theme.popupTintColor
            blurRadius: Theme.popupBlurRadius
            shadowOpacity: Theme.dialogShadowOpacity
        }

        contentItem: Sidebar {
            id: compactSidebar
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
