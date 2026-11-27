pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

Item {
    id: root

    required property Item backgroundSource
    required property Item compactLibraryHost
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
    property bool drawerLibraryActive: false
    property bool peekDismissOnPress: false
    property bool timelineGuideActive: false
    property string documentTitle: ""
    property real sidebarDragWidth: 0
    property bool sidebarResizing: false
    property bool sidebarAnimationReady: false
    property bool suppressSidebarAnimation: false
    readonly property real targetSidebarContentWidth: root.compact
        ? 0
        : root.viewState.sidebarVisible
            ? (root.sidebarResizing ? root.sidebarDragWidth : root.preferences.sidebarWidth)
            : 0
    property real animatedSidebarContentWidth: targetSidebarContentWidth
    readonly property bool sidebarPanelShown: animatedSidebarContentWidth > 0.5
    readonly property bool sidebarAnimating: Math.abs(animatedSidebarContentWidth - targetSidebarContentWidth) > 0.5
    property bool sidebarMaterialHolding: false
    property real sidebarMaterialHoldWidth: 0
    property bool bottomPanelAnimationReady: false
    property real bottomPanelReveal: bottomPanelEffectivelyVisible ? 1 : 0
    readonly property bool bottomPanelShown: bottomPanelReveal > 0.005
    readonly property bool bottomPanelAnimating:
        Math.abs(bottomPanelReveal - (bottomPanelEffectivelyVisible ? 1 : 0)) > 0.005
    readonly property real targetBottomPanelHeight: {
        const maxHeight = Math.max(bottomPanel.minimumHeight, Math.min(
            centerSplit.height * root.preferences.bottomPanelMaximumHeightRatio,
            centerSplit.height - editorHost.SplitView.minimumHeight
                - Theme.splitDividerThickness))
        const minHeight = Math.max(bottomPanel.minimumHeight,
            centerSplit.height * root.preferences.bottomPanelMinimumHeightRatio)
        return Math.max(minHeight, Math.min(maxHeight,
            centerSplit.height * root.preferences.bottomPanelHeightRatio))
    }
    readonly property bool libraryPopupActive: libraryPage.popupActive
    readonly property Item navigationItem: sidebar.navigationItem
    readonly property Item chartSidebarItem: sidebar.chartSidebarItem
    readonly property Item sourceEditorItem: editorPane.sourceEditorItem
    readonly property Item difficultyFieldsItem: editorPane.difficultyFieldsItem
    readonly property WorkbenchPreviewPane preview: previewDetached ? detachedPreview : workspacePreview
    readonly property Item transportItem: preview.transportItem
    readonly property Item timelineGuideItem: bottomPanel.timelineGuideItem
    readonly property Item timelineSettingsGuideItem: bottomPanel.timelineSettingsGuideItem
    readonly property Item renderModeItem: preview.renderModeItem
    readonly property bool renderModeMenuAvailable: preview.renderModeMenuAvailable
    readonly property bool previewDetached: root.preferences.previewDetached
    readonly property Item settingsDialogParent: root.previewDetached
        ? detachedPreviewWindow.Overlay.overlay : root.Overlay.overlay
    property bool previewSurfaceMoving: false
    // Hold the native backdrop at full sidebar width across collapse/expand and
    // for a brief swapchain margin after collapse so AppKit never shrinks ahead
    // of Qt's Metal presentation and exposes unblurred desktop behind the panel.
    readonly property real activityBarMaterialWidth: {
        if (root.compact)
            return 0
        if (!root.preferences.efficientSidebarEnabled)
            return sidebar.activityBarWidth
        if (root.sidebarAnimating)
            return Math.max(sidebar.width, root.targetSidebarContentWidth, root.preferences.sidebarWidth)
        if (root.sidebarResizing)
            return sidebar.visible ? Math.min(horizontalSplit.width, sidebar.width + 64) : 0
        if (sidebar.visible)
            return sidebar.width
        return root.sidebarMaterialHolding ? root.sidebarMaterialHoldWidth : 0
    }
    readonly property rect activityBarMaterialRect: Qt.rect(0, 0,
        activityBarMaterialWidth, horizontalSplit.height)
    readonly property real minimumEditorWidth: Math.max(editorPane.minimumWidth,
        bottomPanel.minimumWidth)
    // 拆分后的工作区按编辑器和底部面板计算，侧栏所需空间单独计算。
    readonly property real minimumWorkspaceWidth: root.previewDetached
        ? minimumEditorWidth
        : Math.max(620, minimumEditorWidth + preview.minimumWidth + Theme.splitDividerThickness)
    readonly property real sidebarChromeWidth: Theme.splitDividerThickness
        + (root.preferences.efficientSidebarEnabled ? 0 : Theme.activityButtonSize)
    readonly property real expandedSidebarWidth: root.preferences.sidebarWidth + root.sidebarChromeWidth
    readonly property real minimumHeight: Math.max(
        editorHost.SplitView.minimumHeight + (root.bottomPanelEffectivelyVisible
            ? bottomPanel.minimumHeight + Theme.splitDividerThickness : 0),
        root.previewDetached ? 0 : preview.minimumHeight)
    readonly property var inputContext: editorPane.inputContext
    readonly property bool canUndo: editorPane.canUndo
    readonly property bool canRedo: editorPane.canRedo
    readonly property bool canCut: editorPane.canCut
    readonly property bool canCopy: editorPane.canCopy
    readonly property bool canPaste: editorPane.canPaste
    readonly property bool canTransform: editorPane.canTransform
    readonly property string selectionBeatStatusText: editorPane.selectionBeatStatusText
    readonly property string selectionBeatTooltipText: editorPane.selectionBeatTooltipText
    // User preference AND backend chart-bottom-tabs mode (export/metadata
    // call setChartBottomTabsMode(false); latency/difficulty turn it back on).
    readonly property bool bottomPanelEffectivelyVisible:
        (root.viewState.difficultyEditorActive || root.viewState.latencyEditorActive)
        && root.viewState.bottomPanelVisible && root.timelineSession.panelVisible
    readonly property bool exportVideoActive:
        root.pages.activePageId === "export"
    readonly property bool libraryActive: root.viewState.activeSidebarView === "library"
    readonly property bool previewOnLeft: root.preferencesModel.previewOnLeft
    onPreviewOnLeftChanged: root.syncWorkspacePanelOrder()
    readonly property real previewEditorAvailableWidth:
        Math.max(1, workspaceSplit.width - (previewHost.visible ? Theme.splitDividerThickness : 0))
    signal openRequested()
    signal settingsRequested()
    signal completionSettingsRequested(string group)
    signal audioSettingsRequested()
    signal previewSettingsRequested()
    signal mediaToolRequested(string toolId)
    signal workspacePressed()

    Behavior on animatedSidebarContentWidth {
        enabled: Theme.animationsEnabled
            && root.sidebarAnimationReady
            && !root.sidebarResizing
            && !root.compact
            && !root.suppressSidebarAnimation
        NumberAnimation {
            duration: Theme.panelSlideDuration
            easing.type: Easing.OutCubic
        }
    }

    Behavior on bottomPanelReveal {
        enabled: Theme.animationsEnabled && root.bottomPanelAnimationReady && !centerSplit.resizing
        NumberAnimation {
            duration: Theme.panelSlideDuration
            easing.type: Easing.OutCubic
        }
    }

    Timer {
        id: sidebarMaterialReleaseTimer
        interval: 60
        onTriggered: root.sidebarMaterialHolding = false
    }

    onSidebarPanelShownChanged: {
        if (!root.compact && root.preferences.efficientSidebarEnabled) {
            if (sidebarPanelShown) {
                sidebarMaterialReleaseTimer.stop()
                root.sidebarMaterialHolding = false
            } else {
                root.sidebarMaterialHoldWidth = Math.max(root.preferences.sidebarWidth, 1)
                root.sidebarMaterialHolding = true
                sidebarMaterialReleaseTimer.restart()
            }
        }
    }

    Component.onCompleted: Qt.callLater(() => {
        root.sidebarAnimationReady = true
        root.bottomPanelAnimationReady = true
    })
    onCompactChanged: {
        root.suppressSidebarAnimation = true
        Qt.callLater(() => { root.suppressSidebarAnimation = false })
    }

    function dockSidebarWithoutAnimation() {
        root.suppressSidebarAnimation = true
        root.viewState.sidebarVisible = true
        root.preferences.sidebarVisible = true
        Qt.callLater(() => { root.suppressSidebarAnimation = false })
    }

    function showMediaToolsMenu() {
        sidebar.showMediaToolsMenu()
    }

    ResizeSnapState {
        id: sidebarSnap
        minimumSize: root.preferences.sidebarMinimumContentWidth
        maximumSize: Math.min(root.preferences.sidebarMaximumContentWidth,
            Math.max(minimumSize, Math.floor(root.width - root.minimumWorkspaceWidth - root.sidebarChromeWidth)))
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

    function toggleRenderModeMenu() {
        if (!preview.renderModeMenuAvailable)
            return
        if (root.previewDetached)
            root.showDetachedPreview()
        preview.toggleRenderModeMenu()
    }

    function showDetachedPreview() {
        if (detachedPreviewWindow.visibility === Window.Minimized)
            detachedPreviewWindow.windowChrome.showRestored()
        detachedPreviewWindow.raise()
        detachedPreviewWindow.requestActivate()
    }

    function detachPreview() {
        preview.closeMenus()
        root.previewSurfaceMoving = true
        root.preferences.previewDetached = true
        detachedPreviewWindow.windowChrome.showRestored()
        root.showDetachedPreview()
        Qt.callLater(root.finishPreviewMove)
    }

    function dockPreview() {
        preview.closeMenus()
        root.previewSurfaceMoving = true
        detachedPreviewWindow.windowChrome.saveWindowState()
        detachedPreviewWindow.hide()
        root.preferences.previewDetached = false
        root.Window.window.requestActivate()
        Qt.callLater(root.finishPreviewMove)
    }

    Connections {
        target: root.Window.window
        function onVisibleChanged() {
            if (root.Window.window.visible && root.previewDetached && !detachedPreviewWindow.visible)
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
        root.sidebarDragWidth = Math.max(sidebarSnap.minimumSize,
            Math.min(sidebarSnap.maximumSize, sidebarSnap.targetSize))
        root.viewState.sidebarVisible = contentWidth >= sidebarSnap.minimumSize / 2
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
        if (!workspaceSplit || workspaceSplit.count !== 2)
            return
        const targetPreviewIndex = root.previewOnLeft ? 0 : 1
        const currentPreviewIndex = workspaceSplit.itemAt(0) === previewHost ? 0 : 1
        if (currentPreviewIndex !== targetPreviewIndex)
            workspaceSplit.moveItem(currentPreviewIndex, targetPreviewIndex)
    }

    component WorkbenchPreviewPane: PreviewPane {
        anchors.fill: parent
        documentAvailable: root.documentSession.hasDocument
        surfaceActive: detached === root.previewDetached
        transportMirrored: root.previewOnLeft
        previewSession: root.previewSession
        preferences: root.preferences
        exportSession: root.pages.exportSession
        exportPageActive: root.exportVideoActive
        latencyActive: root.viewState.latencyEditorActive
                       && root.pages.activePageId === "latency"
        onDetachRequested: root.detachPreview()
        onDockRequested: root.dockPreview()
    }

    WorkbenchPreviewPane {
        id: workspacePreview
        objectName: "workspacePreviewPane"
        parent: previewHost
    }

    WorkbenchPreviewPane {
        id: detachedPreview
        objectName: "detachedPreviewPane"
        parent: detachedPreviewContent
        detached: true
    }

    AppWindow {
        id: detachedPreviewWindow
        objectName: "detachedPreviewWindow"
        platform: root.Window.window ? root.Window.window.platform : null
        windowChrome: root.Window.window
            ? root.Window.window.applicationContext.detachedPreviewWindowChrome : null
        property bool alwaysOnTop: true
        onAlwaysOnTopChanged: if (windowChrome) windowChrome.setAlwaysOnTop(alwaysOnTop)
        staysOnTop: alwaysOnTop
        fallbackColor: Theme.colors.background.panel
        manageTopMaterialRegion: true
        topMaterialThickness: detachedTitleBar.visible
            ? detachedTitleBar.height + detachedToolBar.height : 0
        readonly property bool settingsInTitleBar: Qt.platform.os !== "windows"
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
        onClosing: root.dockPreview()

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
            platform: detachedPreviewWindow.platform
            applicationMenusVisible: false
            titleText: detachedPreviewWindow.captionTitle

            Row {
                id: detachedSettingsActions
                parent: detachedPreviewWindow.settingsInTitleBar ? detachedTitleBar.trailingHost : detachedToolBar
                anchors.right: detachedPreviewWindow.settingsInTitleBar ? undefined : detachedToolBar.right
                anchors.rightMargin: detachedPreviewWindow.settingsInTitleBar ? 0 : 8
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                spacing: Theme.toolbarButtonSpacing
                z: 2

                TextMetrics {
                    id: detachedAudioSettingsMetrics
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    text: qsTrId("action.audio_settings")
                }

                TextMetrics {
                    id: detachedPreviewSettingsMetrics
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    text: qsTrId("action.video_settings")
                }

                readonly property real expandedActionsWidth:
                    Theme.toolbarLabeledButtonWidth(detachedAudioSettingsMetrics)
                    + Theme.toolbarLabeledButtonWidth(detachedPreviewSettingsMetrics)
                    + Theme.toolbarButtonWidth + 2 * Theme.toolbarButtonSpacing
                readonly property bool settingsLabelsCollapsed: detachedPreviewWindow.settingsInTitleBar
                    ? detachedTitleBar.fullTitleWidth > Math.max(0,
                        detachedTitleBar.width
                        - (detachedTitleBar.leadingToolAreaWidth + detachedTitleBar.menuGap)
                        - (detachedTitleBar.captionButtonsWidth + expandedActionsWidth + 8 + detachedTitleBar.menuGap))
                    : detachedToolBar.width < expandedActionsWidth + 16

                component DetachedToolButton: IconButton {
                    width: label.length > 0 ? implicitWidth : Theme.toolbarButtonWidth
                    height: Math.min(Theme.toolbarButtonHeight, detachedSettingsActions.height)
                    iconWidth: Theme.toolbarButtonIconSize
                    iconHeight: Theme.toolbarButtonIconSize
                    anchors.verticalCenter: parent.verticalCenter
                    stateColors: Theme.chromeStateColorsFor(detachedPreviewWindow.nativeMaterialActive)
                }

                DetachedToolButton {
                    objectName: "detachedPreviewAudioSettingsButton"
                    iconSource: Qt.resolvedUrl("icons/audio-settings.svg")
                    label: detachedSettingsActions.settingsLabelsCollapsed ? "" : qsTrId("action.audio_settings")
                    tooltip: qsTrId("action.audio_settings")
                    onClicked: root.audioSettingsRequested()
                }

                DetachedToolButton {
                    objectName: "detachedPreviewPreviewSettingsButton"
                    iconSource: Qt.resolvedUrl("icons/preview-settings.svg")
                    label: detachedSettingsActions.settingsLabelsCollapsed ? "" : qsTrId("action.video_settings")
                    tooltip: qsTrId("action.video_settings")
                    onClicked: root.previewSettingsRequested()
                }

                DetachedToolButton {
                    objectName: "detachedPreviewAlwaysOnTopButton"
                    iconSource: Qt.resolvedUrl("icons/pin.svg")
                    filledIconSource: Qt.resolvedUrl("icons/pin-filled.svg")
                    highlightWhenActive: false
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

        Shortcut {
            sequences: [StandardKey.Close]
            enabled: root.Window.window.windowCommandsEnabled
            onActivated: detachedPreviewWindow.close()
        }

        PreviewShortcuts {
            shortcuts: root.Window.window.applicationContext.shortcuts
            previewSession: root.previewSession
            inputContext: root.Window.window.inputContext
            commandsEnabled: root.Window.window.windowCommandsEnabled
                && root.Window.window.workbenchCommandsAllowed
            playbackCommandsEnabled: root.Window.window.playbackCommandsEnabled
        }

        UiScaleShortcuts {
            shortcuts: root.Window.window.applicationContext.shortcuts
            preferencesModel: root.preferencesModel
            commandsEnabled: root.Window.window.windowCommandsEnabled
        }

        Shortcut {
            sequence: "Escape"
            enabled: root.Window.window.windowCommandsEnabled
                && detachedPreviewWindow.visibility === Window.FullScreen
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
            compact: root.compact
            expandedContentWidth: root.sidebarResizing ? root.sidebarDragWidth : root.preferences.sidebarWidth
            contentRevealed: root.sidebarPanelShown
            visible: !root.compact && (!root.preferences.efficientSidebarEnabled || root.sidebarPanelShown)
            width: visible ? sidebar.activityBarWidth + root.animatedSidebarContentWidth : 0
            height: parent.height
            onSettingsRequested: root.settingsRequested()
            onMediaToolRequested: toolId => root.mediaToolRequested(toolId)
        }

        // Behind the split in opaque mode, so a fractional device pixel at the
        // divider shows the panel surface instead of the window base. Hidden
        // when custom wallpaper is active so translucent panes are never
        // double-tinted over the wallpaper.
        Rectangle {
            x: workspaceSplit.x
            y: workspaceSplit.y
            width: workspaceSplit.width
            height: workspaceSplit.height
            visible: !Theme.backgroundActive
            color: Theme.surfaceColor(Theme.colors.background.panel)
        }

        SplitView {
            id: workspaceSplit
            orientation: Qt.Horizontal
            x: sidebar.width + sidebarHandle.width
            width: parent.width - x
            height: parent.height
            Component.onCompleted: root.syncWorkspacePanelOrder()

            handle: SplitHandle {
                verticalDivider: true
                snapped: previewWidthSnap.constrained
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
                    snapped: bottomPanelSnap.constrained && !bottomPanelSnap.lineConstrained
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

                    // Passive bottom-edge hover zone so the closed bottom panel can be
                    // reopened in place without obstructing editor text, scrollbars, or clicks.
                    Item {
                        id: bottomPanelRevealZone
                        readonly property bool available:
                            (root.viewState.difficultyEditorActive || root.viewState.latencyEditorActive)
                            && root.timelineSession.panelVisible
                            && !root.viewState.bottomPanelVisible
                            && !root.bottomPanelAnimating
                            && !root.pages.overlayActive
                            && !root.exportVideoActive
                        readonly property bool reveal: available
                            && !centerSplit.resizing
                            && (bottomRevealHover.hovered || bottomRevealPill.hovered || bottomRevealPill.visualFocus)

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: 112
                        height: 36
                        z: 5
                        visible: available

                        HoverHandler {
                            id: bottomRevealHover
                            blocking: false
                        }

                        SidebarEdgePill {
                            id: bottomRevealPill
                            buttonObjectName: "bottomPanelRevealButton"
                            horizontal: true
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 8
                            iconSource: Qt.resolvedUrl("icons/chevron-down.svg")
                            iconRotation: 180
                            tooltip: qsTrId("qml.open_bottom_panel")
                            interactive: bottomPanelRevealZone.reveal
                            opacity: bottomPanelRevealZone.reveal ? 1 : 0
                            visible: opacity > 0.01

                            Behavior on opacity {
                                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                            }

                            onClicked: {
                                root.viewState.bottomPanelVisible = true
                                root.preferences.bottomPanelVisible = true
                            }
                        }
                    }

                }

                BottomPanel {
                    id: bottomPanel
                    enabled: root.bottomPanelEffectivelyVisible
                    visible: root.bottomPanelShown
                    expandedHeight: root.bottomPanelAnimating ? root.targetBottomPanelHeight : 0
                    guideSettingsActive: root.timelineGuideActive
                    documentSession: root.documentSession
                    analysisSession: root.analysisSession
                    preferences: root.preferences
                    timelineSession: root.timelineSession
                    previewSession: root.previewSession
                    SplitView.minimumHeight: root.bottomPanelAnimating
                        ? 0
                        : bottomPanelSnap.constrained
                            ? bottomPanelSnap.targetSize : bottomPanelSnap.minimumSize
                    SplitView.maximumHeight: root.bottomPanelAnimating
                        ? Math.max(0, root.targetBottomPanelHeight)
                        : bottomPanelSnap.constrained
                            ? bottomPanelSnap.targetSize : bottomPanelSnap.maximumSize
                    onAnalysisRowActivated: (difficultyId, revision, line, column, endColumn, second) =>
                        editorPane.revealAnalysisRow(
                            difficultyId, revision, line, column, endColumn, second, root.analysisSession)
                    onCloseRequested: {
                        root.viewState.bottomPanelVisible = false
                        root.preferences.bottomPanelVisible = false
                    }
                }
            }

            Item {
                id: previewHost
                visible: !root.previewDetached
                SplitView.minimumWidth: previewWidthSnap.constrained
                    ? previewWidthSnap.targetSize : previewWidthSnap.minimumSize
                SplitView.maximumWidth: previewWidthSnap.constrained
                    ? previewWidthSnap.targetSize : previewWidthSnap.maximumSize
            }
        }

        // Passively observe workspace presses above workspaceSplit while the
        // floating drawer is open so clicking the editor dismisses it without
        // swallowing the click (and never adds an extra SplitView pane).
        Item {
            x: workspaceSplit.x
            y: workspaceSplit.y
            width: workspaceSplit.width
            height: workspaceSplit.height
            z: 10
            visible: root.peekDismissOnPress

            PointHandler {
                enabled: root.peekDismissOnPress
                acceptedButtons: Qt.AllButtons
                onActiveChanged: {
                    if (active)
                        root.workspacePressed()
                }
            }
        }

        ChartLibraryPage {
            id: libraryPage
            objectName: "chartLibraryPage"
            parent: root.compact || root.drawerLibraryActive ? root.compactLibraryHost : sidebar.libraryHost
            anchors.fill: parent
            visible: root.libraryActive
            compact: root.compact || root.drawerLibraryActive
            efficientMode: root.preferences.efficientSidebarEnabled
            nativeMaterialActive: !compact && root.preferences.efficientSidebarEnabled && Theme.nativeMaterialActive
            activeChartPath: root.documentSession.currentFilePath
            onChartRequested: (chartUrl, preferredDifficultyId) => root.commands.openLibraryChart(chartUrl, preferredDifficultyId)
            onChartFolderRequested: chartUrl => root.commands.openChartFolder(chartUrl)
        }

        Connections {
            target: root.commands
            function onLibraryChartOpened(difficultyId) {
                root.viewState.openDifficultyEditor(difficultyId)
                if (root.compact && root.libraryActive)
                    root.viewState.compactPanel = ""
            }
        }

        SplitSnapController {
            id: previewWidthSnap
            x: workspaceSplit.x
            y: workspaceSplit.y
            splitView: workspaceSplit
            resizedItem: previewHost
            resizeDirection: root.previewOnLeft ? 1 : -1
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
            snapAvailable: dragAvailable && !root.bottomPanelAnimating
            readonly property bool lineSnapAvailable: dragAvailable
                && !root.previewDetached && !root.previewSurfaceMoving
                && previewHost.visible && preview.visible && root.transportItem.visible
                && root.transportItem.width > 0 && root.transportItem.height > 0
            readonly property real activeLineCenterOffset:
                lineCenterOffset(Theme.splitHandleActiveThickness)
            readonly property real idleLineCenterOffset:
                lineCenterOffset(1 / (root.Screen.devicePixelRatio * Theme.uiScale))
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
            readonly property bool showingEditorCorner: (root.compact || !root.sidebarPanelShown)
                && !(root.previewOnLeft && previewHost.visible)
                && !root.exportVideoActive
            visible: sidebar.visible && !root.preferences.efficientSidebarEnabled
            x: root.compact ? 0 : sidebar.activityBarWidth
            backgroundSource: root.backgroundSource
            backgroundOffset: Qt.point(root.backgroundOffset.x + x, root.backgroundOffset.y)
            panelItem: !root.compact && root.sidebarPanelShown
                ? (sidebar.libraryActive ? libraryPage.cornerSourceItem : sidebar.cornerSourceItem)
                : root.previewOnLeft && previewHost.visible
                    ? preview.cornerSourceItem
                    : root.exportVideoActive ? exportVideoPage.cornerSourceItem
                        : editorPane.cornerSourceItem
            panelOffset: Qt.point(0, 0)
            panelBackingColor: showingEditorCorner
                ? editorPane.cornerBackingColor
                : Theme.surfaceColor(Theme.colors.background.panel)
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

        SplitHandle {
            id: sidebarHandle
            verticalDivider: true
            x: sidebar.width
            width: visible && root.sidebarPanelShown ? Theme.splitDividerThickness : 0
            height: parent.height
            // In collapsed efficient mode, keep the left edge free for hover-peek
            // instead of showing a split resize cursor on the editor edge.
            visible: !root.compact && (!root.preferences.efficientSidebarEnabled || root.sidebarPanelShown)
            showDivider: root.sidebarPanelShown
            snapped: sidebarSnap.snapped
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

        // Passive hover zone straddling the split divider center so the
        // floating collapse pill appears on approach without blocking scroll or clicks.
        Item {
            id: efficientCollapseZone
            readonly property bool available: !root.compact
                && root.preferences.efficientSidebarEnabled
                && root.viewState.sidebarVisible
                && !root.sidebarAnimating
            readonly property bool reveal: available
                && !root.sidebarResizing
                && !sidebarDrag.pressed
                && (edgeHover.hovered || sidebarDrag.containsMouse
                    || efficientCollapsePill.hovered || efficientCollapsePill.visualFocus)

            x: sidebarHandle.x + sidebarHandle.width / 2 - width / 2
            y: Math.round((parent.height - height) / 2)
            width: 36
            height: 96
            z: 5
            visible: available

            HoverHandler {
                id: edgeHover
            }

            SidebarEdgePill {
                id: efficientCollapsePill
                buttonObjectName: "efficientSidebarCollapseButton"
                anchors.centerIn: parent
                iconSource: Qt.resolvedUrl("icons/chevron-left.svg")
                tooltip: qsTrId("qml.close_sidebar")
                interactive: efficientCollapseZone.reveal
                opacity: efficientCollapseZone.reveal ? 1 : 0
                visible: opacity > 0.01

                Behavior on opacity {
                    NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                }

                onClicked: {
                    root.viewState.sidebarVisible = false
                    root.preferences.sidebarVisible = false
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
        value: root.bottomPanelAnimating
            ? root.targetBottomPanelHeight * root.bottomPanelReveal
            : root.bottomPanelEffectivelyVisible
                ? centerSplit.height * root.preferences.bottomPanelHeightRatio : 0
        when: !centerSplit.resizing
        restoreMode: Binding.RestoreNone
    }

    // Defer installing the export preview audition until the bottom panel exit
    // animation finishes so its render-thread work does not stall the collapse.
    Binding {
        target: root.pages.exportSession
        property: "auditionDeferred"
        value: root.bottomPanelAnimating
        when: root.pages && root.pages.exportSession !== null
        restoreMode: Binding.RestoreNone
    }
}
