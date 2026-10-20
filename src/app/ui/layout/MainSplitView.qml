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
    required property var net
    required property var netUpload
    property bool compact: false
    property string documentTitle: ""
    property real sidebarDragWidth: 0
    property bool sidebarResizing: false
    readonly property Item navigationItem: sidebar.navigationItem
    readonly property Item chartSidebarItem: sidebar.chartSidebarItem
    readonly property Item sourceEditorItem: editorPane.sourceEditorItem
    readonly property Item difficultyFieldsItem: editorPane.difficultyFieldsItem
    readonly property Item transportItem: preview.transportItem
    readonly property Item renderModeItem: preview.renderModeItem
    readonly property bool previewDetached: root.preferences.previewDetached
    readonly property Item settingsDialogParent: root.previewDetached
        ? detachedPreviewWindow.Overlay.overlay : root.Overlay.overlay
    property bool previewSurfaceMoving: false
    readonly property rect activityBarMaterialRect: Qt.rect(0, 0,
        root.compact ? 0 : root.preferences.efficientSidebarEnabled
            ? sidebar.width + sidebarToggle.width : sidebar.activityBarWidth,
        horizontalSplit.height)
    readonly property real minimumEditorWidth: Math.max(editorPane.minimumWidth,
        bottomPanel.minimumWidth)
    // 拆分后的工作区按编辑器和底部面板计算，侧栏所需空间单独计算。
    readonly property real minimumWorkspaceWidth: root.previewDetached
        ? minimumEditorWidth
        : Math.max(620, minimumEditorWidth + preview.minimumWidth + Theme.splitDividerThickness)
    readonly property real expandedSidebarWidth:
        sidebar.activityBarWidth + root.preferences.sidebarWidth + Theme.splitDividerThickness
        + (root.preferences.efficientSidebarEnabled ? 20 : 0)
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
    signal completionSettingsRequested(string group)
    signal audioSettingsRequested()
    signal previewSettingsRequested()
    signal mediaToolRequested(string toolId)

    function showMediaToolsMenu() {
        sidebar.showMediaToolsMenu()
    }

    ResizeSnapState {
        id: sidebarSnap
        minimumSize: root.preferences.sidebarMinimumContentWidth
        maximumSize: root.preferences.sidebarMaximumContentWidth
        targets: [{ size: root.preferences.sidebarDefaultWidth }]
        alignmentTolerance: 0.5 / root.Screen.devicePixelRatio
    }

    function persistBottomPanelHeightRatio(finalHeight, sizeAtPress) {
        if (!root.bottomPanelEffectivelyVisible || centerSplit.height <= 0)
            return
        const height = Math.max(bottomPanelSnap.minimumSize,
            Math.min(bottomPanelSnap.maximumSize, finalHeight))
        if (height === sizeAtPress)
            return
        // 比例按基础约束保存，吸附期间的固定高度属于临时布局约束。
        root.preferences.bottomPanelHeightRatio =
            height <= bottomPanelSnap.minimumSize
                ? root.preferences.bottomPanelMinimumHeightRatio
                : height >= bottomPanelSnap.maximumSize
                    ? root.preferences.bottomPanelMaximumHeightRatio
                    : height / centerSplit.height
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
        sidebarSnap.update(contentWidth)
        root.sidebarDragWidth = Math.max(root.preferences.sidebarMinimumContentWidth,
            Math.min(root.preferences.sidebarMaximumContentWidth,
                     sidebarSnap.snapped ? sidebarSnap.targetSize : contentWidth))
        root.viewState.sidebarVisible = contentWidth >= root.preferences.sidebarMinimumContentWidth / 2
    }

    function persistPreviewWidthRatio(finalWidth) {
        if (root.previewDetached || root.previewSurfaceMoving
                || !previewHost.visible || !centerSplit.visible
                || root.previewEditorAvailableWidth <= 0)
            return
        const width = Math.max(previewWidthSnap.minimumSize,
            Math.min(previewWidthSnap.maximumSize, finalWidth))
        root.preferences.previewWidthRatio = width / root.previewEditorAvailableWidth
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
        surfaceActive: !root.previewSurfaceMoving && (root.previewDetached || !root.viewState.netEditorActive)
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
        readonly property Item backdropSource: detachedSceneContent
        property var windowChrome: null
        property bool alwaysOnTop: true
        onAlwaysOnTopChanged: if (windowChrome) windowChrome.setAlwaysOnTop(alwaysOnTop)
        onWindowChromeChanged: if (windowChrome) windowChrome.setAlwaysOnTop(alwaysOnTop)
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
        minimumWidth: Math.min(Screen.desktopAvailableWidth, Math.ceil(preview.minimumWidth * Theme.uiScale))
        minimumHeight: Math.min(Screen.desktopAvailableHeight,
            Math.ceil((preview.minimumHeight + detachedTitleBar.height + detachedToolBar.height) * Theme.uiScale))
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
                    (detachedTitleBar.height + detachedToolBar.height) * Theme.uiScale) }]
                : []
            restoreMode: Binding.RestoreNone
        }

        UiScaleSurface {
            id: detachedSceneContent

            WindowBackground {
                anchors.fill: parent
                nativeMaterialActive: detachedPreviewWindow.nativeMaterialActive
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
                    ? detachedPreviewWindow.windowChrome.titleBarHeight / Theme.uiScale : 0
                leadingInset: detachedPreviewWindow.windowChrome
                    ? detachedPreviewWindow.windowChrome.titleBarLeadingInset / Theme.uiScale : 0
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

                    IconButton {
                        objectName: "detachedPreviewAlwaysOnTopButton"
                        height: Math.min(implicitHeight, detachedSettingsActions.height)
                        anchors.verticalCenter: parent.verticalCenter
                        stateColors: Theme.chromeStateColorsFor(detachedPreviewWindow.nativeMaterialActive)
                        iconSource: Qt.resolvedUrl("icons/pin.svg")
                        filledIconSource: Qt.resolvedUrl("icons/pin-filled.svg")
                        checkable: true
                        checked: detachedPreviewWindow.alwaysOnTop
                        tooltip: checked ? qsTrId("preview.window.disable_always_on_top")
                                         : qsTrId("preview.window.always_on_top")
                        Accessible.name: qsTrId("preview.window.always_on_top")
                        onToggled: detachedPreviewWindow.alwaysOnTop = checked
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

            Item {
                id: detachedPreviewContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: detachedToolBar.bottom
                anchors.bottom: parent.bottom
            }
        }

        Shortcut {
            sequences: [StandardKey.Close]
            onActivated: detachedPreviewWindow.close()
        }

        UiScaleShortcuts {
            shortcuts: root.Window.window.applicationContext.shortcuts
            preferencesModel: root.preferencesModel
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
            visible: !root.compact && (!root.preferences.efficientSidebarEnabled || root.viewState.sidebarVisible)
            width: visible ? sidebar.activityBarWidth + (root.viewState.sidebarVisible
                ? (root.sidebarResizing ? root.sidebarDragWidth : root.preferences.sidebarWidth) : 0) : 0
            height: parent.height
            onSettingsRequested: root.settingsRequested()
            onMediaToolRequested: toolId => root.mediaToolRequested(toolId)
        }

        SplitView {
            id: workspaceSplit
            orientation: Qt.Horizontal
            x: sidebar.width + sidebarHandle.width + sidebarToggle.width
            width: parent.width - x
            height: parent.height
            Component.onCompleted: root.syncWorkspacePanelOrder()

            handle: SplitHandle {
                verticalDivider: true
                onReleased: previewWidthSnap.finishDrag(true)
            }

            SplitView {
                id: centerSplit
                orientation: Qt.Vertical

                SplitView.fillWidth: true
                SplitView.minimumWidth: previewHost.visible
                    ? root.previewEditorAvailableWidth * (1 - root.preferences.previewMaximumWidthRatio)
                    : root.minimumEditorWidth
                SplitView.maximumWidth: previewHost.visible
                    ? root.previewEditorAvailableWidth * (1 - root.preferences.previewMinimumWidthRatio) : Infinity

                handle: SplitHandle {
                    verticalDivider: false
                    onReleased: bottomPanelSnap.finishDrag(true)
                }

                Item {
                    id: editorHost
                    clip: true
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
                        previewSession: root.previewSession
                        net: root.net
                        netUpload: root.netUpload
                        pages: root.pages
                        onOpenRequested: root.openRequested()
                        onCompletionSettingsRequested: function(group) { root.completionSettingsRequested(group) }
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
                    SplitView.minimumHeight: bottomPanelSnap.constrained
                        ? bottomPanelSnap.targetSize : bottomPanelSnap.minimumSize
                    SplitView.maximumHeight: bottomPanelSnap.constrained
                        ? bottomPanelSnap.targetSize : bottomPanelSnap.maximumSize
                    onAnalysisRowActivated: (difficultyId, revision, line, column, endColumn, second) =>
                        editorPane.revealAnalysisRow(
                            difficultyId, revision, line, column, endColumn, second, root.analysisSession)
                }
            }

            Item {
                id: previewHost
                visible: !root.previewDetached && !root.viewState.netEditorActive
                SplitView.minimumWidth: previewWidthSnap.constrained
                    ? previewWidthSnap.targetSize : previewWidthSnap.minimumSize
                SplitView.maximumWidth: previewWidthSnap.constrained
                    ? previewWidthSnap.targetSize : previewWidthSnap.maximumSize
            }
        }

        SplitSnapController {
            id: previewWidthSnap
            x: workspaceSplit.x
            y: workspaceSplit.y
            splitView: workspaceSplit
            resizedItem: previewHost
            resizeDirection: root.preferencesModel.previewOnLeft ? 1 : -1
            minimumSize: root.previewEditorAvailableWidth * root.preferences.previewMinimumWidthRatio
            maximumSize: root.previewEditorAvailableWidth * root.preferences.previewMaximumWidthRatio
            targets: [{ size: root.previewEditorAvailableWidth * root.preferences.previewDefaultWidthRatio }]
            snapAvailable: dragAvailable && !root.previewDetached && !root.previewSurfaceMoving
                && centerSplit.visible
            onFinished: finalSize => root.persistPreviewWidthRatio(finalSize)
        }

        SplitSnapController {
            id: bottomPanelSnap
            x: workspaceSplit.x + workspaceSplit.contentItem.x + centerSplit.x
            y: workspaceSplit.y + workspaceSplit.contentItem.y + centerSplit.y
            splitView: centerSplit
            resizedItem: bottomPanel
            resizeDirection: -1
            minimumSize: root.bottomPanelEffectivelyVisible
                ? Math.max(bottomPanel.minimumHeight,
                    centerSplit.height * root.preferences.bottomPanelMinimumHeightRatio) : 0
            maximumSize: root.bottomPanelEffectivelyVisible
                ? Math.max(bottomPanel.minimumHeight, Math.min(
                    centerSplit.height * root.preferences.bottomPanelMaximumHeightRatio,
                    centerSplit.height - editorHost.SplitView.minimumHeight
                        - Theme.splitDividerThickness)) : 0
            snapAvailable: dragAvailable
            readonly property bool lineSnapAvailable: dragAvailable
                && !root.previewDetached && !root.previewSurfaceMoving
                && previewHost.visible && preview.visible && root.transportItem.visible
                && root.transportItem.width > 0 && root.transportItem.height > 0
            readonly property real activeLineCenterOffset:
                lineCenterOffset(Theme.splitHandleActiveThickness)
            readonly property real idleLineCenterOffset:
                lineCenterOffset(1 / root.Screen.devicePixelRatio)
            // 两个宿主属于同一 SplitView；显式坐标依赖随预览布局变化更新。
            readonly property real targetCenter: lineSnapAvailable
                ? previewHost.y + preview.y + root.transportItem.y
                    + root.transportItem.topEdgeCenterY - centerSplit.y : NaN
            readonly property bool lineConstrained: constrained && snappedTargetIndex === 1
            targets: {
                const values = [{ size: centerSplit.height * root.preferences.bottomPanelDefaultHeightRatio }]
                if (lineSnapAvailable) {
                    values.push({
                        size: centerSplit.height - targetCenter + activeLineCenterOffset,
                        releaseSize: centerSplit.height - targetCenter + idleLineCenterOffset,
                        enterDistance: 4,
                        leaveDistance: 7
                    })
                }
                return values
            }
            onFinished: (finalSize, sizeAtPress) =>
                root.persistBottomPanelHeightRatio(finalSize, sizeAtPress)

            function lineCenterOffset(thickness) {
                return -Theme.splitDividerThickness
                    + Math.floor((Theme.splitDividerThickness - thickness)
                                 * root.Screen.devicePixelRatio / 2) / root.Screen.devicePixelRatio
                    + thickness / 2
            }
        }

        // 覆盖层连接两侧高亮，横线主体由原生分割槽绘制。
        Rectangle {
            x: workspaceSplit.x + workspaceSplit.contentItem.x + centerSplit.x
                + (previewHost.x < centerSplit.x ? -width : centerSplit.width)
            y: workspaceSplit.y + workspaceSplit.contentItem.y + centerSplit.y
                + centerSplit.contentItem.y + bottomPanel.y
                + bottomPanelSnap.activeLineCenterOffset - height / 2
            width: Theme.splitDividerThickness
            height: Theme.splitHandleActiveThickness
            z: 1
            visible: bottomPanelSnap.lineConstrained
            color: Theme.colors.accent.focus
            enabled: false
        }

        CornerMask {
            id: workspaceCorner
            visible: sidebar.visible && !root.preferences.efficientSidebarEnabled
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
            panelBackingColor: panelItem === editorPane.cornerSourceItem
                ? "transparent" : Theme.surfaceColor(Theme.colors.background.panel)
        }

        Rectangle {
            x: workspaceCorner.visible ? workspaceCorner.x + workspaceCorner.width : 0
            y: 0
            width: parent.width - x
            height: 1 / root.Screen.devicePixelRatio
            color: workspaceCorner.separatorColor
            enabled: false
        }

        Rectangle {
            x: workspaceCorner.x
            y: workspaceCorner.visible ? workspaceCorner.height : 0
            width: 1 / root.Screen.devicePixelRatio
            height: parent.height - y
            visible: sidebar.visible && !root.preferences.efficientSidebarEnabled
            color: workspaceCorner.separatorColor
            enabled: false
        }

        Rectangle {
            id: sidebarToggle
            x: sidebar.width
            y: 1 / root.Screen.devicePixelRatio
            width: visible ? 20 : 0
            height: parent.height - y
            visible: !root.compact && root.preferences.efficientSidebarEnabled
            color: Theme.chromeSurfaceColor(Theme.colors.background.panel)

            IconButton {
                id: sidebarToggleButton
                objectName: "efficientSidebarToggleButton"
                anchors.centerIn: parent
                width: parent.width
                height: 32
                iconWidth: 14
                iconHeight: 14
                Accessible.name: tooltip
                background: Rectangle {
                    radius: Theme.compactControlRadius
                    color: Theme.overlayColor(sidebarToggleButton.down
                        ? Theme.chromeStateColors.pressed
                        : sidebarToggleButton.hovered || sidebarToggleButton.visualFocus
                            ? Theme.chromeStateColors.hover : "transparent")
                }
                iconSource: root.viewState.sidebarVisible
                    ? Qt.resolvedUrl("icons/chevron-left.svg")
                    : Qt.resolvedUrl("icons/chevron-right.svg")
                tooltip: root.viewState.sidebarVisible
                    ? qsTrId("qml.close_sidebar") : qsTrId("qml.open_sidebar")
                onClicked: {
                    root.viewState.sidebarVisible = !root.viewState.sidebarVisible
                    root.preferences.sidebarVisible = root.viewState.sidebarVisible
                }
            }
        }

        SplitHandle {
            id: sidebarHandle
            verticalDivider: true
            x: sidebar.width + sidebarToggle.width
            width: visible && (root.viewState.sidebarVisible || root.preferences.efficientSidebarEnabled)
                ? Theme.splitDividerThickness : 0
            height: parent.height
            visible: !root.compact
            showDivider: root.viewState.sidebarVisible || root.preferences.efficientSidebarEnabled
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
                    sidebarSnap.begin(startWidth)
                    root.sidebarResizing = true
                }
                onPositionChanged: mouse => {
                    if (pressed)
                        root.resizeSidebar(startWidth + mapToItem(horizontalSplit, mouse.x, mouse.y).x - startX)
                }
                onReleased: {
                    root.persistSidebarWidth()
                    sidebarSnap.end()
                    root.sidebarResizing = false
                }
                onCanceled: {
                    root.persistSidebarWidth()
                    sidebarSnap.end()
                    root.sidebarResizing = false
                }
            }
        }
    }

    Binding {
        target: root.transportItem
        property: "topEdgeHighlighted"
        value: bottomPanelSnap.lineConstrained
        restoreMode: Binding.RestoreNone
    }

    // 分离期间保留宽度比例，回归或主窗口缩放时按当前可用宽度恢复。
    Binding {
        target: previewHost.SplitView
        property: "preferredWidth"
        value: root.previewEditorAvailableWidth * root.preferences.previewWidthRatio
        when: !workspaceSplit.resizing
        restoreMode: Binding.RestoreNone
    }

    // 原生 SplitView 管理拖动高度；松开后接回持久比例绑定。
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
