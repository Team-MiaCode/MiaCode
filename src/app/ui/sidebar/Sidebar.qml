import QtQuick
import MiaCode.UI

Item {
    id: root

    required property var viewState
    required property var documentSession
    required property var preferences
    required property var commands
    required property var pages
    property bool compact: false
    property real expandedContentWidth: 0
    property bool contentRevealed: compact || viewState.sidebarVisible
    property string pendingChartEditor: ""
    property bool pendingLibrary: false
    readonly property bool efficientMode: preferences.efficientSidebarEnabled
    readonly property bool libraryActive: viewState.activeSidebarView === "library"
    readonly property bool primarySidebarVisible: compact || contentRevealed
    readonly property bool popupActive: activityBar.toolsMenuActive || efficientSidebar.popupActive
    readonly property alias libraryHost: libraryContent
    readonly property Item navigationItem: efficientMode
        ? (libraryActive ? libraryContent : efficientSidebar) : activityBar
    readonly property Item chartSidebarItem: efficientMode
        ? efficientSidebar.navigationContentItem : chartSidebar.navigationContentItem
    readonly property real activityBarWidth: activityBar.width
    readonly property Item cornerSourceItem: efficientMode ? efficientHeader
        : libraryActive ? libraryContent
        : root.viewState.activeSidebarView === "export" ? exportSidebar.cornerSourceItem
        : root.viewState.activeSidebarView === "network" ? networkSidebar.cornerSourceItem
        : root.viewState.activeSidebarView === "tools" ? toolsSidebar.cornerSourceItem
        : chartSidebar.cornerSourceItem

    signal settingsRequested()
    signal mediaToolRequested(string toolId)

    function showMediaToolsMenu() {
        if (root.efficientMode) {
            root.viewState.activeSidebarView = "chart"
            if (!root.compact) {
                root.viewState.sidebarVisible = true
                root.preferences.sidebarVisible = true
            }
            Qt.callLater(efficientSidebar.showTools)
        } else {
            root.viewState.activeSidebarView = "tools"
            if (!root.compact) {
                root.viewState.sidebarVisible = true
                root.preferences.sidebarVisible = true
            }
        }
    }

    clip: true

    function showLibrary() {
        if (!root.compact) {
            viewState.sidebarVisible = true
            preferences.sidebarVisible = true
        }
        viewState.activeSidebarView = "library"
        const lastEditor = viewState.lastChartEditor()
        if (documentSession.hasDocument && !viewState.difficultyEditorActive && lastEditor.length > 0)
            viewState.activateEditor(lastEditor)
    }

    // Activity Bar 只负责功能域导航。再次选择当前功能时，桌面端切换
    // Primary Sidebar；紧凑布局直接关闭当前覆盖层。
    function activateView(viewId) {
        if (viewId === "library") {
            if (root.pages.navigationPending)
                return
            if (!root.efficientMode && viewState.activeSidebarView === "library" && !root.pages.overlayActive) {
                if (compact) {
                    viewState.compactPanel = ""
                    return
                }
                viewState.sidebarVisible = !viewState.sidebarVisible
                root.preferences.sidebarVisible = viewState.sidebarVisible
                return
            }
            if (root.pages.overlayActive) {
                root.pendingLibrary = true
                if (!root.pages.leaveOverlayPage())
                    root.pendingLibrary = false
            } else {
                root.showLibrary()
            }
            return
        }
        const pageAlreadyActive = viewId === "export" ? root.pages.activePageId === "export"
            : viewId === "chart" ? !root.pages.overlayActive && !root.viewState.netEditorActive
            : true
        if (viewState.activeSidebarView === viewId && pageAlreadyActive) {
            if (compact) {
                viewState.compactPanel = ""
                return
            }
            viewState.sidebarVisible = !viewState.sidebarVisible
            root.preferences.sidebarVisible = viewState.sidebarVisible
            return
        }

        if (viewId === "chart" && root.documentSession.hasDocument) {
            const lastChartEditor = root.viewState.lastChartEditor()
            if (root.pages.overlayActive) {
                root.pendingChartEditor = lastChartEditor
                root.pages.rememberEditorReturnTarget(lastChartEditor)
                if (!root.pages.leaveOverlayPage())
                    root.pendingChartEditor = ""
            } else if (lastChartEditor.length > 0) {
                root.viewState.activateEditor(lastChartEditor)
            }
        }

        viewState.activeSidebarView = viewId
        if (!root.compact && !viewState.sidebarVisible) {
            viewState.sidebarVisible = true
            root.preferences.sidebarVisible = true
        }

        if (viewId === "export" && root.documentSession.hasDocument) {
            root.pages.rememberEditorReturnTarget(root.viewState.activeEditorKey)
            root.pages.openVideoExportPage()
        }
    }

    Connections {
        target: root.pages
        function onOverlayPageLeft() {
            if (root.pendingLibrary) {
                root.pendingLibrary = false
                root.showLibrary()
                return
            }
            const key = root.pendingChartEditor
            root.pendingChartEditor = ""
            if (key.length > 0 && root.viewState.containsEditor(key))
                root.viewState.activateEditor(key)
        }
        function onNavigationRejected() {
            root.pendingLibrary = false
            root.pendingChartEditor = ""
        }
    }

    ActivityBar {
        netUploadEnabled: root.pages.netUploadEnabled
        id: activityBar
        z: 2
        visible: !root.efficientMode
        width: !root.efficientMode ? implicitWidth : 0
        color: root.compact ? "transparent" : Theme.chromeSurfaceColor(Theme.colors.background.activityBar)
        buttonStateColors: root.compact ? Theme.colors.popupState : Theme.chromeStateColors
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        activeView: root.viewState.activeSidebarView
        documentAvailable: root.documentSession.hasDocument
        networkActive: root.viewState.netEditorActive
        toolsAvailable: true
        onViewRequested: viewId => root.activateView(viewId)
        onToolRequested: function(toolId) {
            if (toolId === "net-download" || toolId === "net-upload") {
                root.pages.openNetPage(toolId)
                return
            }
            if (!root.documentSession.hasDocument)
                return
            if (toolId.startsWith("media."))
                root.mediaToolRequested(toolId.substring(6))
        }
        onSettingsRequested: root.settingsRequested()
    }

    // Clip sliding sidebar content at its expanded width so collapse/expand
    // slides smoothly without reflowing or squishing labels and cards.
    Item {
        id: pageHostClip
        anchors.left: activityBar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true
        visible: root.primarySidebarVisible && width > 0.5

        Item {
            id: pageHost
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.compact || root.expandedContentWidth <= 0
                ? parent.width
                : Math.max(parent.width, root.expandedContentWidth)

        Rectangle {
            id: efficientHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: visible ? Theme.workspaceHeaderHeight + 8 : 0
            visible: root.efficientMode
            color: root.compact ? "transparent"
                : Theme.chromeSurfaceColor(Theme.colors.background.panel, Theme.nativeMaterialActive)

            AppSegmentedControl {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                nativeMaterialActive: !root.compact && Theme.nativeMaterialActive
                tabs: [
                    { id: "menu", label: qsTrId("sidebar.menu"), objectName: "efficientMenuTab" },
                    { id: "library", label: qsTrId("library.title"), objectName: "efficientLibraryTab" }
                ]
                selectedId: root.libraryActive ? "library" : "menu"
                onTabSelected: tabId => {
                    if (tabId === "library")
                        root.activateView("library")
                    else
                        root.viewState.activeSidebarView = "chart"
                }
            }
        }

        Rectangle {
            id: libraryContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: root.efficientMode ? efficientHeader.bottom : parent.top
            anchors.bottom: parent.bottom
            visible: root.libraryActive
            color: root.compact ? "transparent" : root.efficientMode
                ? Theme.chromeSurfaceColor(Theme.colors.background.panel, Theme.nativeMaterialActive)
                : Theme.surfaceColor(Theme.colors.background.panel)
        }

        ChartFieldSidebar {
            id: chartSidebar
            color: root.compact ? "transparent" : Theme.surfaceColor(Theme.colors.background.panel)
            rowStateColors: root.compact ? Theme.colors.popupState : Theme.colors.listState
            anchors.fill: parent
            visible: !root.efficientMode && root.viewState.activeSidebarView === "chart"
            viewState: root.viewState
            documentSession: root.documentSession
            commands: root.commands
            pages: root.pages
        }

        EfficientSidebar {
            id: efficientSidebar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: efficientHeader.bottom
            anchors.bottom: parent.bottom
            visible: root.efficientMode && !root.libraryActive
            compact: root.compact
            nativeMaterialActive: !root.compact && Theme.nativeMaterialActive
            color: root.compact ? "transparent"
                : Theme.chromeSurfaceColor(Theme.colors.background.panel, Theme.nativeMaterialActive)
            viewState: root.viewState
            documentSession: root.documentSession
            commands: root.commands
            pages: root.pages
            toolsMenuActive: activityBar.toolsMenuActive
            onToolsRequested: activityBar.showMediaToolsMenu(efficientSidebar.toolsItem)
            onSettingsRequested: root.settingsRequested()
        }

        UtilitySidebarPage {
            id: networkSidebar
            color: root.compact ? "transparent" : Theme.surfaceColor(Theme.colors.background.panel)
            rowStateColors: root.compact ? Theme.colors.popupState : Theme.colors.listState
            anchors.fill: parent
            visible: !root.efficientMode && root.viewState.activeSidebarView === "network"
            mode: "network"
            pages: root.pages
            documentAvailable: root.documentSession.hasDocument
        }

        UtilitySidebarPage {
            id: toolsSidebar
            color: root.compact ? "transparent" : Theme.surfaceColor(Theme.colors.background.panel)
            rowStateColors: root.compact ? Theme.colors.popupState : Theme.colors.listState
            anchors.fill: parent
            visible: !root.efficientMode && root.viewState.activeSidebarView === "tools"
            mode: "tools"
            pages: root.pages
            documentAvailable: root.documentSession.hasDocument
            onToolRequested: toolId => root.mediaToolRequested(toolId)
        }

        ExportSidebarPage {
            id: exportSidebar
            color: root.compact ? "transparent" : Theme.surfaceColor(Theme.colors.background.panel)
            rowStateColors: root.compact ? Theme.colors.popupState : Theme.colors.listState
            anchors.fill: parent
            visible: !root.efficientMode && root.viewState.activeSidebarView === "export"
            documentAvailable: root.documentSession.hasDocument
            pages: root.pages
        }
        }
    }
}
