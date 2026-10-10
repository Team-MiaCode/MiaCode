pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

Item {
    id: root

    required property Item backgroundSource
    required property point backgroundOffset
    required property var viewState
    required property var documentSession
    required property var analysisSession
    required property var preferences
    required property var previewSession
    required property var previewSettings
    required property var commands
    required property var timelineSession
    required property var preferencesModel
    required property var pages
    required property var editorController
    required property var editorSync
    required property var latency
    property bool compact: false
    property string documentTitle: ""
    property real sidebarDragWidth: 0
    property bool sidebarResizing: false
    readonly property bool previewDetached: root.preferences.previewDetached
    readonly property Item settingsDialogParent: root.previewDetached
        ? detachedPreviewWindow.Overlay.overlay : root.Overlay.overlay
    property bool previewSurfaceMoving: false
    readonly property rect activityBarMaterialRect: Qt.rect(0, 0,
        root.compact ? 0 : sidebar.activityBarWidth,
        horizontalSplit.height)
    readonly property real minimumEditorWidth: Math.max(editorPane.minimumWidth,
        bottomPanel.minimumWidth)
    // 拆分后的工作区按编辑器和底部面板计算，侧栏所需空间单独计算。
    readonly property real minimumWorkspaceWidth: root.previewDetached
        ? minimumEditorWidth
        : Math.max(620, minimumEditorWidth + preview.minimumWidth + Theme.splitDividerThickness)
    readonly property real expandedSidebarWidth:
        sidebar.activityBarWidth + root.preferences.sidebarWidth + Theme.splitDividerThickness
    readonly property real minimumHeight: Math.max(
        editorHost.SplitView.minimumHeight + (root.bottomPanelEffectivelyVisible
            ? bottomPanel.minimumHeight + Theme.splitDividerThickness : 0),
        root.previewDetached ? 0 : preview.minimumHeight)
    readonly property bool canUndo: editorPane.canUndo
    readonly property bool canRedo: editorPane.canRedo
    readonly property bool canCut: editorPane.canCut
    readonly property bool canCopy: editorPane.canCopy
    readonly property bool canPaste: editorPane.canPaste
    readonly property bool canTransform: editorPane.canTransform
    readonly property bool normalizationAvailable: editorPane.normalizationAvailable
    readonly property string selectionBeatStatusText: editorPane.selectionBeatStatusText
    readonly property string selectionBeatTooltipText: editorPane.selectionBeatTooltipText
    // User preference AND backend chart-bottom-tabs mode (export/metadata
    // call setChartBottomTabsMode(false); latency/difficulty turn it back on).
    readonly property bool bottomPanelEffectivelyVisible:
        (root.viewState.difficultyEditorActive || root.viewState.latencyEditorActive)
        && root.viewState.bottomPanelVisible && root.timelineSession.panelVisible
    readonly property bool exportVideoActive:
        root.pages.activePageId === "export"
    readonly property real previewEditorAvailableWidth:
        Math.max(1, workspaceSplit.width - (previewHost.visible ? Theme.splitDividerThickness : 0))
    signal openRequested()
    signal settingsRequested()
    signal audioSettingsRequested()
    signal previewSettingsRequested()
    signal mediaToolRequested(string toolId)

    function showMediaToolsMenu() {
        sidebar.showMediaToolsMenu()
    }

    function persistBottomPanelHeightRatio() {
        if (!root.bottomPanelEffectivelyVisible || centerSplit.height <= 0
                || !centerSplit.resizing || bottomPanel.height === centerSplit.panelHeightAtPress)
            return
        // 拖到边界表达比例的最小/最大值，临时的绝对高度约束留在布局层。
        root.preferences.bottomPanelHeightRatio =
            bottomPanel.height <= bottomPanel.SplitView.minimumHeight
                ? root.preferences.bottomPanelMinimumHeightRatio
                : bottomPanel.height >= bottomPanel.SplitView.maximumHeight
                    ? root.preferences.bottomPanelMaximumHeightRatio
                    : bottomPanel.height / centerSplit.height
    }

    function undo() {
        editorPane.undo()
    }

    function redo() {
        editorPane.redo()
    }

    function cut() {
        editorPane.cut()
    }

    function copy() {
        editorPane.copy()
    }

    function paste() {
        editorPane.paste()
    }

    function requestCloseActiveEditor() {
        editorPane.requestCloseActiveEditor()
    }

    function selectAll() {
        editorPane.selectAll()
    }

    function showFindReplace() {
        editorPane.openFindReplace()
    }

    function selectCurrentLine() {
        editorPane.selectCurrentLine()
    }

    function canNormalizeChart() {
        return editorPane.canNormalizeChart()
    }

    function normalizationSelectionDescription() {
        return editorPane.normalizationSelectionDescription()
    }

    function applyNormalization(options) {
        return editorPane.applyNormalization(options)
    }

    function applyChartTransform(opId) {
        return editorPane.applyChartTransform(opId)
    }

    function detachPreview() {
        preview.closeMenus()
        root.previewSurfaceMoving = true
        root.preferences.previewDetached = true
        detachedPreviewWindow.windowChrome.showRestored()
        detachedPreviewWindow.raise()
        detachedPreviewWindow.requestActivate()
        Qt.callLater(root.finishPreviewMove)
    }

    function dockPreview() {
        preview.closeMenus()
        root.previewSurfaceMoving = true
        detachedPreviewWindow.windowChrome.saveWindowState()
        root.preferences.previewDetached = false
        detachedPreviewWindow.hide()
        root.Window.window.requestActivate()
        Qt.callLater(root.finishPreviewMove)
    }

    Connections {
        target: root.Window.window
        function onVisibleChanged() {
            if (root.Window.window.visible && root.previewDetached)
                root.detachPreview()
        }
    }

    function finishPreviewMove() {
        root.previewSurfaceMoving = false
        preview.forceActiveFocus()
    }

    function persistSidebarWidth() {
        if (root.compact) return
        if (root.viewState.sidebarVisible)
            root.preferences.sidebarWidth = Math.round(root.sidebarDragWidth)
        root.preferences.sidebarVisible = root.viewState.sidebarVisible
    }

    function resizeSidebar(contentWidth) {
        root.sidebarDragWidth = Math.max(root.preferences.sidebarMinimumContentWidth,
            Math.min(root.preferences.sidebarMaximumContentWidth, contentWidth))
        root.viewState.sidebarVisible = contentWidth >= root.preferences.sidebarMinimumContentWidth / 2
    }

    function persistPreviewWidthRatio() {
        if (root.compact || root.previewDetached) return
        root.preferences.previewWidthRatio = previewHost.width / root.previewEditorAvailableWidth
    }

    function syncWorkspacePanelOrder() {
        const targetPreviewIndex = root.preferencesModel.previewOnLeft ? 0 : 1
        const currentPreviewIndex = workspaceSplit.itemAt(0) === previewHost ? 0 : 1
        if (currentPreviewIndex !== targetPreviewIndex)
            workspaceSplit.moveItem(currentPreviewIndex, targetPreviewIndex)
    }

    Connections {
        target: root.preferencesModel
        function onInterfaceChanged() {
            root.syncWorkspacePanelOrder()
        }
    }

    PreviewPane {
        id: preview
        parent: root.previewDetached ? detachedPreviewContent : previewHost
        anchors.fill: parent
        documentAvailable: root.documentSession.hasDocument
        surfaceActive: !root.previewSurfaceMoving
        detached: root.previewDetached
        previewSession: root.previewSession
        preferences: root.preferences
        exportSession: root.pages.exportSession
        exportPageActive: root.exportVideoActive
        latencyActive: root.viewState.latencyEditorActive
                       && root.pages.activePageId === "latency"
        onDetachRequested: root.detachPreview()
        onDockRequested: root.dockPreview()
    }

    ApplicationWindow {
        id: detachedPreviewWindow
        objectName: "detachedPreviewWindow"
        readonly property Item backdropSource: detachedPreviewContent
        property var windowChrome: null
        readonly property bool settingsInTitleBar: Qt.platform.os !== "windows"
        readonly property bool nativeMaterialActive: windowChrome
            && windowChrome.nativeMaterialAvailable && Theme.blurMaterialsEnabled
            && !Theme.backgroundActive
        readonly property string captionTitle: root.documentTitle.length > 0
            ? (root.documentSession.dirty ? "* " : "") + root.documentTitle
            : qsTrId("preview.window.title")
        title: captionTitle.replace(/ — /g, " - ") + (root.documentTitle.length > 0
            ? " - " + qsTrId("preview.window.title") : "")
        transientParent: null
        visible: false
        width: 560
        height: 680
        minimumWidth: preview.minimumWidth
        minimumHeight: preview.minimumHeight + detachedTitleBar.height + detachedToolBar.height
        flags: {
            let value = Qt.Window
            if (root.Window.window.platform.captionButtons) {
                value |= Qt.CustomizeWindowHint
                        | Qt.WindowTitleHint
                        | Qt.WindowSystemMenuHint
                        | Qt.WindowMinimizeButtonHint
                        | Qt.WindowMaximizeButtonHint
                        | Qt.WindowCloseButtonHint
            }
            if (Qt.platform.os === "linux")
                value |= Qt.FramelessWindowHint
            if (Qt.platform.os === "osx")
                value |= Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
            return value
        }
        // Allocate the alpha surface before WindowChrome creates the native handle.
        color: Qt.platform.os === "osx" || Qt.platform.os === "windows"
            ? "transparent" : Theme.colors.background.panel
        background: null
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        topPadding: 0
        leftPadding: 0
        rightPadding: 0
        bottomPadding: 0
        palette.window: Theme.colors.background.surface
        palette.windowText: Theme.colors.text.primary
        palette.base: Theme.colors.background.surface
        palette.text: Theme.colors.text.primary
        palette.button: Theme.colors.background.panel
        palette.buttonText: Theme.colors.text.primary
        palette.highlight: Theme.colors.state.textSelection
        palette.highlightedText: Theme.colors.text.active
        palette.disabled.text: Theme.colors.text.disabled
        palette.disabled.buttonText: Theme.colors.text.disabled
        onClosing: root.dockPreview()

        Binding {
            target: detachedPreviewWindow.windowChrome
            property: "materialRegions"
            when: (Qt.platform.os === "osx" || Qt.platform.os === "windows") && target !== null
            value: detachedPreviewWindow.visible && detachedTitleBar.visible
                && Theme.blurMaterialsEnabled && !Theme.backgroundActive
                ? [{ rect: Qt.rect(0, 0, detachedPreviewWindow.width,
                    detachedTitleBar.height + detachedToolBar.height) }]
                : []
            restoreMode: Binding.RestoreNone
        }

        WindowTitleBar {
            id: detachedTitleBar
            color: Theme.chromeSurfaceColor(Theme.colors.background.titleBar,
                detachedPreviewWindow.nativeMaterialActive)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            visible: detachedPreviewWindow.visibility !== Window.FullScreen
            height: visible ? implicitHeight : 0
            hostWindow: detachedPreviewWindow
            windowChrome: detachedPreviewWindow.windowChrome
            platform: root.Window.window.platform
            applicationMenusVisible: false
            titleText: detachedPreviewWindow.captionTitle
            nativeHeight: detachedPreviewWindow.windowChrome
                ? detachedPreviewWindow.windowChrome.titleBarHeight : 0
            leadingInset: detachedPreviewWindow.windowChrome
                ? detachedPreviewWindow.windowChrome.titleBarLeadingInset : 0
            leadingToolAreaWidth: leadingInset
            trailingToolAreaWidth: detachedPreviewWindow.settingsInTitleBar
                ? detachedSettingsActions.width + 8 : 0

            Row {
                id: detachedSettingsActions
                parent: detachedPreviewWindow.settingsInTitleBar ? detachedTitleBar : detachedToolBar
                anchors.right: parent.right
                anchors.rightMargin: detachedPreviewWindow.settingsInTitleBar
                    ? detachedTitleBar.captionButtonsWidth + 8 : 8
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                spacing: 5
                z: 2

                IconButton {
                    objectName: "detachedPreviewAudioSettingsButton"
                    height: Math.min(implicitHeight, detachedSettingsActions.height)
                    anchors.verticalCenter: parent.verticalCenter
                    stateColors: Theme.chromeStateColorsFor(detachedPreviewWindow.nativeMaterialActive)
                    iconSource: Qt.resolvedUrl("icons/audio-settings.svg")
                    label: qsTrId("action.audio_settings")
                    tooltip: qsTrId("action.audio_settings")
                    onClicked: root.audioSettingsRequested()
                }

                IconButton {
                    objectName: "detachedPreviewPreviewSettingsButton"
                    height: Math.min(implicitHeight, detachedSettingsActions.height)
                    anchors.verticalCenter: parent.verticalCenter
                    stateColors: Theme.chromeStateColorsFor(detachedPreviewWindow.nativeMaterialActive)
                    iconSource: Qt.resolvedUrl("icons/preview-settings.svg")
                    label: qsTrId("action.video_settings")
                    tooltip: qsTrId("action.video_settings")
                    onClicked: root.previewSettingsRequested()
                }
            }
        }

        Rectangle {
            id: detachedToolBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: detachedTitleBar.bottom
            visible: !detachedPreviewWindow.settingsInTitleBar && detachedTitleBar.visible
            height: visible ? Theme.windowChromeRowHeight : 0
            color: Theme.chromeSurfaceColor(Theme.colors.background.activityBar,
                detachedPreviewWindow.nativeMaterialActive)

            WindowGestureArea {
                anchors.fill: parent
                hostWindow: detachedPreviewWindow
                windowChrome: detachedPreviewWindow.windowChrome
            }
        }

        Rectangle {
            id: detachedPreviewContent
            color: Theme.colors.background.panel
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: detachedToolBar.bottom
            anchors.bottom: parent.bottom
        }

        Shortcut {
            sequence: StandardKey.Close
            onActivated: detachedPreviewWindow.close()
        }

        Shortcut {
            sequence: "Escape"
            enabled: detachedPreviewWindow.visibility === Window.FullScreen
            onActivated: detachedPreviewWindow.windowChrome.showRestored()
        }
    }

    Item {
        id: horizontalSplit
        anchors.fill: parent
        readonly property int orientation: Qt.Horizontal

        Sidebar {
            id: sidebar
            viewState: root.viewState
            documentSession: root.documentSession
            preferences: root.preferences
            commands: root.commands
            pages: root.pages
            compact: false
            visible: !root.compact
            width: visible ? sidebar.activityBarWidth + (root.viewState.sidebarVisible
                ? (root.sidebarResizing ? root.sidebarDragWidth : root.preferences.sidebarWidth) : 0) : 0
            height: parent.height
            onSettingsRequested: root.settingsRequested()
            onMediaToolRequested: toolId => root.mediaToolRequested(toolId)
        }

        SplitView {
            id: workspaceSplit
            orientation: Qt.Horizontal
            x: sidebar.width + sidebarHandle.width
            width: parent.width - x
            height: parent.height
            Component.onCompleted: root.syncWorkspacePanelOrder()

            handle: SplitHandle {
                onReleased: root.persistPreviewWidthRatio()
            }

            SplitView {
                id: centerSplit
                orientation: Qt.Vertical
                property real panelHeightAtPress: 0
                onResizingChanged: {
                    if (resizing)
                        panelHeightAtPress = bottomPanel.height
                }
                SplitView.fillWidth: true
                SplitView.minimumWidth: root.previewDetached ? root.minimumEditorWidth
                    : Math.max(root.minimumEditorWidth,
                    Math.min(root.previewEditorAvailableWidth * (1.0 - root.preferences.previewMaximumWidthRatio),
                             root.previewEditorAvailableWidth - preview.minimumWidth))

                handle: SplitHandle {
                    onReleased: root.persistBottomPanelHeightRatio()
                }

                Item {
                    id: editorHost
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 180

                    EditorPane {
                        id: editorPane
                        anchors.fill: parent
                        visible: !root.pages.overlayActive && !root.exportVideoActive
                        editorController: root.editorController
                        editorSync: root.editorSync
                        analysisSession: root.analysisSession
                        viewState: root.viewState
                        documentSession: root.documentSession
                        commands: root.commands
                        preferences: root.preferences
                        latency: root.latency
                        pages: root.pages
                        onOpenRequested: root.openRequested()
                    }

                    // v2 video export center: QML chrome + ExportVideoController panel surface.
                    ExportVideoPage {
                        id: exportVideoPage
                        anchors.fill: parent
                        visible: root.exportVideoActive
                        pages: root.pages
                        previewSession: root.previewSession
                        previewSettings: root.previewSettings
                    }

                }

                BottomPanel {
                    id: bottomPanel
                    visible: root.bottomPanelEffectivelyVisible
                    documentSession: root.documentSession
                    analysisSession: root.analysisSession
                    preferences: root.preferences
                    timelineSession: root.timelineSession
                    previewSession: root.previewSession
                    SplitView.minimumHeight: root.bottomPanelEffectivelyVisible
                                             ? Math.max(bottomPanel.minimumHeight,
                                                 centerSplit.height * root.preferences.bottomPanelMinimumHeightRatio)
                                             : 0
                    SplitView.maximumHeight: root.bottomPanelEffectivelyVisible
                                             ? Math.max(bottomPanel.minimumHeight, Math.min(
                                                 centerSplit.height * root.preferences.bottomPanelMaximumHeightRatio,
                                                 centerSplit.height - editorHost.SplitView.minimumHeight
                                                     - Theme.splitDividerThickness))
                                             : 0
                    onAnalysisRowActivated: (difficultyId, revision, line, column, endColumn, second) =>
                        editorPane.revealAnalysisRow(
                            difficultyId, revision, line, column, endColumn, second, root.analysisSession)
                }
            }

            Item {
                id: previewHost
                visible: !root.previewDetached
                SplitView.minimumWidth: Math.max(preview.minimumWidth,
                    Math.min(root.previewEditorAvailableWidth * root.preferences.previewMinimumWidthRatio,
                             root.previewEditorAvailableWidth - bottomPanel.minimumWidth))
                SplitView.maximumWidth: Math.max(preview.minimumWidth,
                    Math.min(root.previewEditorAvailableWidth * root.preferences.previewMaximumWidthRatio,
                             root.previewEditorAvailableWidth - bottomPanel.minimumWidth))
            }
        }

        CornerMask {
            id: workspaceCorner
            visible: sidebar.visible
            x: root.compact ? 0 : sidebar.activityBarWidth
            backgroundSource: root.backgroundSource
            backgroundOffset: Qt.point(root.backgroundOffset.x + x, root.backgroundOffset.y)
            panelItem: !root.compact && root.viewState.sidebarVisible
                ? sidebar.cornerSourceItem
                : root.preferencesModel.previewOnLeft && previewHost.visible
                    ? preview.cornerSourceItem
                    : root.exportVideoActive ? exportVideoPage.cornerSourceItem
                        : editorPane.cornerSourceItem
            panelOffset: Qt.point(0, 0)
            panelBaseColor: Theme.colors.background.panel
        }

        Rectangle {
            x: workspaceCorner.visible ? workspaceCorner.x + workspaceCorner.width : 0
            y: 0
            width: parent.width - x
            height: 1 / root.Screen.devicePixelRatio
            color: Theme.chromeSeparatorColor
            enabled: false
        }

        Rectangle {
            x: workspaceCorner.x
            y: workspaceCorner.height
            width: 1 / root.Screen.devicePixelRatio
            height: parent.height - y
            visible: sidebar.visible
            color: Theme.chromeSeparatorColor
            enabled: false
        }

        SplitHandle {
            id: sidebarHandle
            x: sidebar.width
            width: visible && root.viewState.sidebarVisible ? Theme.splitDividerThickness : 0
            height: parent.height
            visible: !root.compact
            showDivider: root.viewState.sidebarVisible
            handlePressed: sidebarDrag.pressed
            handleHovered: sidebarDrag.containsMouse

            MouseArea {
                id: sidebarDrag
                anchors.centerIn: parent
                width: Theme.splitHandleHitExtent
                height: parent.height
                hoverEnabled: true
                cursorShape: Qt.SplitHCursor
                preventStealing: true
                property real startX: 0
                property real startWidth: 0

                onPressed: mouse => {
                    startX = mapToItem(horizontalSplit, mouse.x, mouse.y).x
                    startWidth = root.viewState.sidebarVisible ? root.preferences.sidebarWidth : 0
                    root.sidebarDragWidth = root.preferences.sidebarWidth
                    root.sidebarResizing = true
                }
                onPositionChanged: mouse => {
                    if (pressed)
                        root.resizeSidebar(startWidth + mapToItem(horizontalSplit, mouse.x, mouse.y).x - startX)
                }
                onReleased: {
                    root.persistSidebarWidth()
                    root.sidebarResizing = false
                }
                onCanceled: {
                    root.persistSidebarWidth()
                    root.sidebarResizing = false
                }
            }
        }
    }

    // 分离期间保留宽度比例，回归或主窗口缩放时按当前可用宽度恢复。
    Binding {
        target: previewHost.SplitView
        property: "preferredWidth"
        value: root.previewEditorAvailableWidth * root.preferences.previewWidthRatio
        when: !workspaceSplit.resizing
        restoreMode: Binding.RestoreNone
    }

    // SplitView 拖动时会写入首选高度；松开后重新接回持久比例绑定。
    // 缩小窗口触及高度下限后，放大仍按用户原有比例计算。
    Binding {
        target: bottomPanel.SplitView
        property: "preferredHeight"
        value: root.bottomPanelEffectivelyVisible
            ? centerSplit.height * root.preferences.bottomPanelHeightRatio : 0
        when: !centerSplit.resizing
        restoreMode: Binding.RestoreNone
    }
}
