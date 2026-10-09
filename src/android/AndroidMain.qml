import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI
import "qrc:/preview/runtime/qml" as Preview

ApplicationWindow {
    id: window
    width: 1280
    height: 720
    visible: true
    title: "MiaCode Mobile"
    color: Theme.colors.background.surface
    property Item backdropSource: sceneContent
    readonly property string documentTitle: {
        if (!androidSession.hasDocument) return ""
        const metadataTitle = androidSession.title.trim()
        const baseTitle = metadataTitle.length > 0 ? metadataTitle : androidSession.currentFileName
        return state.difficultyEditorActive
            ? baseTitle + " — " + androidSession.currentDifficultyLabel : baseTitle
    }
    property string activePage: "chart"
    property bool coverOpen: false
    onCoverOpenChanged: {
        if (coverOpen) mobileCover.enter()
        else mobileCover.leave()
    }
    onActivePageChanged: {
        if (activePage === "export") mobileExport.enter()
        else mobileExport.leave()
        if (activePage !== "chart") mobileLatency.leave()
        else if (state.latencyEditorActive) mobileLatency.enter()
    }
    readonly property bool showSidebar: mobilePreferences.sidebarVisible
    readonly property bool showBottom: mobilePreferences.bottomPanelVisible
    property bool sidebarResizing: false
    property real sidebarDragWidth: mobilePreferences.sidebarWidth
    property bool sidebarDragVisible: mobilePreferences.sidebarVisible
    property bool previewPointerResizing: false
    readonly property bool sidebarEffectivelyVisible: sidebarResizing ? sidebarDragVisible : showSidebar
    readonly property bool bottomPanelEffectivelyVisible: showBottom && window.activePage === "chart"
        && (state.difficultyEditorActive || state.latencyEditorActive)
    readonly property real previewEditorAvailableWidth: Math.max(1, workspaceSplit.width - Theme.splitDividerThickness)
    property bool previewFullscreen: false
    property bool sourceEditorOverlayHeld: false
    property bool sourceEditorFocused: false
    readonly property bool previewOnLeft: mobilePreferencesStore.panelsSwapped
    // Fit the complete v2 workbench using the actual panes' minimum geometry.
    // Painting and touch hit testing share the same Item transform.
    readonly property real workbenchScale: Math.min(1,
        sceneContent.width / Math.max(activityBar.implicitWidth + mobilePreferences.sidebarMaximumContentWidth
            + Math.max(300, bottomPanel.minimumWidth) + previewPane.minimumWidth + 2 * Theme.splitDividerThickness,
            coverLoader.item ? coverLoader.item.implicitWidth : 0),
        sceneContent.height / (titleBar.implicitHeight + mainToolbar.implicitHeight
            + Math.max(previewPane.stableMinimumHeight, 180 + bottomPanel.minimumHeight + Theme.splitDividerThickness)
            + statusBar.implicitHeight))

    // v2 MainSplitView keeps transient geometry during a drag and persists on
    // release. The stable fitting budget above covers the whole sidebar range,
    // so changing a divider cannot change the pointer's coordinate transform.
    function resizeSidebar(contentWidth) {
        sidebarDragWidth = Math.max(mobilePreferences.sidebarMinimumContentWidth,
            Math.min(mobilePreferences.sidebarMaximumContentWidth, contentWidth))
        sidebarDragVisible = contentWidth >= mobilePreferences.sidebarMinimumContentWidth / 2
    }
    function persistSidebarWidth() {
        if (sidebarDragVisible) mobilePreferences.sidebarWidth = Math.round(sidebarDragWidth)
        mobilePreferences.sidebarVisible = sidebarDragVisible
    }
    function persistPreviewWidthRatio() {
        if (!workspaceSplit.previewDragStarted || previewPane.width === workspaceSplit.previewWidthAtPress) return
        mobilePreferences.previewWidthRatio = previewPane.width / previewEditorAvailableWidth
    }
    function syncWorkspacePanelOrder() {
        const targetIndex = window.previewOnLeft ? 0 : 1
        const currentIndex = workspaceSplit.itemAt(0) === previewPane ? 0 : 1
        if (targetIndex !== currentIndex) workspaceSplit.moveItem(currentIndex, targetIndex)
    }
    onPreviewOnLeftChanged: Qt.callLater(syncWorkspacePanelOrder)
    function persistBottomPanelHeightRatio() {
        if (!bottomPanelEffectivelyVisible || centerSplit.height <= 0 || !centerSplit.resizing
            || bottomPanel.height === centerSplit.panelHeightAtPress) return
        mobilePreferences.bottomPanelHeightRatio = bottomPanel.height <= bottomPanel.SplitView.minimumHeight
            ? mobilePreferences.bottomPanelMinimumHeightRatio
            : bottomPanel.height >= bottomPanel.SplitView.maximumHeight
                ? mobilePreferences.bottomPanelMaximumHeightRatio : bottomPanel.height / centerSplit.height
    }

    // Popup.Item reparents its visual content to the window overlay. Keep that
    // layer in the same logical coordinate space as the fitted v2 workbench.
    Overlay.overlay.objectName: "mobileWorkbenchOverlay"
    Overlay.overlay.x: window.contentItem.x
    Overlay.overlay.y: window.contentItem.y
    Overlay.overlay.width: sceneContent.width / window.workbenchScale
    Overlay.overlay.height: sceneContent.height / window.workbenchScale
    Overlay.overlay.transform: Scale {
        xScale: window.workbenchScale
        yScale: window.workbenchScale
    }

    QtObject {
        id: mobilePlatform
        property bool embeddedMenuInTitleBar: true
        property bool nativeMenuBar: false
        property bool captionButtons: false
    }
    QtObject { id: mobilePet; property bool visible: false }
    MainMenuCommands {
        id: menuCommands
        canUndo: v2EditorController.canUndo
        canRedo: v2EditorController.canRedo
        canCut: editorPane.canCut
        canCopy: editorPane.canCopy
        canPaste: editorPane.canPaste
        onNewDocumentRequested: window.confirmReplace("new")
        onOpenRequested: openProjectDialog.open()
        onSaveRequested: androidSession.save()
        onSaveWholeDocumentRequested: androidSession.save()
        onSaveAsRequested: androidSession.save(true)
        onUndoRequested: editorPane.undo()
        onRedoRequested: editorPane.redo()
        onCutRequested: editorPane.cut()
        onCopyRequested: editorPane.copy()
        onPasteRequested: editorPane.paste()
        onSelectAllRequested: editorPane.selectAll()
        onFindRequested: editorPane.openFindReplace()
        onSelectCurrentLineRequested: editorPane.selectCurrentLine()
        onChartTransformRequested: opId => editorPane.applyChartTransform(opId)
        onNormalizeChartRequested: normalizeDialog.open()
        onMetadataRequested: pages.activateMetadataPage()
        onLatencyCalibrationRequested: pages.openLatencyPage()
        onPreferencesRequested: settings.open()
        onAudioSettingsRequested: audioSettingsDialog.open()
        onPreviewSettingsRequested: previewSettingsDialog.open()
        onMediaToolsRequested: assetsDialog.open()
        onPreviewRateStepRequested: direction => mobilePreview.rate = Math.max(0.25, Math.min(2, mobilePreview.rate + direction * 0.25))
        onCloseDocumentRequested: window.confirmReplace("new")
        onExitRequested: { androidSession.flushRecovery(); Qt.quit() }
        onAboutRequested: aboutDialog.open()
    }
    NormalizeOptionsDialog {
        id: normalizeDialog
        documentSession: androidSession
        selectionDescription: editorPane.normalizationSelectionDescription()
        onAccepted: Qt.callLater(() => editorPane.applyNormalization({
            reduceTo384Grid: reduceTo384Grid, sectionMeasureCount: sectionMeasureCount, syntax: syntax
        }))
    }
    ChoiceDialog {
        id: aboutDialog
        title: "MiaCode Mobile"
        message: "MiaCode v2 Android 迁移开发版"
        choices: [{ id: "close", label: "关闭" }]
        dismissChoiceId: "close"
    }
    QtObject {
        id: mobileRangePreview
        readonly property bool available: window.activePage === "export" && session.activeTab === "export" && session.settingsTab === "output"
        onAvailableChanged: if (!available) active = false
        property bool active: false
        onActiveChanged: mobilePreview.setPlaybackRangeEnabled(active && available, session.exportStartSeconds, session.exportEndSeconds)
        property bool armed: false
        property real startSeconds: 0
        readonly property var session: mobileExport.session
    }
    ViewState {
        id: state
        onDifficultyEditorActivationRequested: id => { window.activePage = "chart"; androidSession.selectDifficulty(id) }
        onLatencyEditorActiveChanged: {
            if (latencyEditorActive && window.activePage === "chart") mobileLatency.enter()
            else mobileLatency.leave()
        }
    }
    QtObject {
        id: commands
        function addDifficulty(id) { androidSession.addDifficulty(id); return androidSession.currentDifficultyId === id }
        function removeDifficulty(id) { androidSession.removeDifficulty(id) }
        function applyDesignerSlots(slots, unified, name) { androidSession.applyDesignerSlots(slots, unified, name) }
        function newDocument() { window.confirmReplace("new") }
    }
    QtObject {
        id: pages
        objectName: "mobilePages"
        readonly property string activePageId: window.activePage === "export" ? "export"
            : state.latencyEditorActive ? "latency" : ""
        readonly property bool overlayActive: false
        readonly property var exportSession: mobileExport.session
        function activateMetadataPage() { window.activePage = "chart"; state.openMetadataEditor(); return true }
        function activateDifficultyPage() { window.activePage = "chart"; state.openDifficultyEditor(androidSession.currentDifficultyId) }
        function openVideoExportPage() { window.activePage = "export" }
        function openCoverExport() { window.coverOpen = true }
        function packAsZip() { mobileZipExport.requestExport() }
        function openLatencyPage() { window.activePage = "chart"; state.openLatencyEditor() }
    }
    Connections {
        target: mobileExport.session
        function onRangeChanged() {
            if (mobileRangePreview.active) mobilePreview.setPlaybackRangeEnabled(true, target.exportStartSeconds, target.exportEndSeconds)
        }
    }
    Component.onCompleted: {
        Qt.callLater(syncWorkspacePanelOrder)
        Theme.preferences = mobilePreferences
        Theme.appBackground = mobileAppBackground
        state.resetEditorTabs(androidSession.currentDifficultyId)
        if (androidSession.recoveryAvailable) recovery.open()
    }
    Connections {
        target: androidSession
        function onDocumentReplaced() { window.coverOpen = false; state.resetEditorTabs(androidSession.currentDifficultyId) }
        function onDocumentStateChanged() { state.syncDifficultyEditors(androidSession.difficulties, androidSession.currentDifficultyId) }
        function onDifficultyCloseRequested(id) { state.closeEditor(state.difficultyEditorKey(id)) }
        function onBookmarkNavigationRequested(id, line) {
            mobileEditorSync.requestNavigation(id, androidSession.documentRevision,
                androidSession.chartPosition(line, 1), androidSession.chartPosition(line, 1), true, true)
        }
    }

    // Direct children preserve wallpaper -> workspace -> fullscreen paint order.
    // Dialogs stay in Overlay so backdrop capture cannot sample its own effect.
    Rectangle {
        id: sceneContent
        objectName: "mobileSceneContent"
        anchors.fill: parent
        color: Theme.colors.background.surface
        ApplicationBackground {
            anchors.fill: parent
            appBackground: mobileAppBackground
        }
    Item {
        id: workbench
        objectName: "mobileWorkbenchRoot"
        // ApplicationWindow content already excludes its safe-area padding.
        // Fit painting and pointer coordinates to that same available region.
        width: sceneContent.width / window.workbenchScale
        height: sceneContent.height / window.workbenchScale
        scale: window.workbenchScale
        transformOrigin: Item.TopLeft
    ColumnLayout {
        anchors.fill: parent
        visible: !window.coverOpen
        spacing: 0
        WindowTitleBar {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            hostWindow: window
            menuCommands: menuCommands
            shortcuts: mobileShortcuts
            documentSession: androidSession
            platform: mobilePlatform
            pet: mobilePet
            documentTitle: window.documentTitle
            saveEnabled: !androidSession.busy
            toolCommandsEnabled: false
        }
        MainToolBar {
            id: mainToolbar
            objectName: "mobileMainToolbar"
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            hostWindow: window
            sidebarActive: window.showSidebar
            bottomActive: window.bottomPanelEffectivelyVisible
            bottomPanelEnabled: state.difficultyEditorActive || state.latencyEditorActive
            canUndo: v2EditorController.canUndo
            canRedo: v2EditorController.canRedo
            saveEnabled: !androidSession.busy
            onOpenRequested: openProjectDialog.open()
            onSaveRequested: androidSession.save()
            onUndoRequested: editorPane.undo()
            onRedoRequested: editorPane.redo()
            onToggleSidebarRequested: mobilePreferences.sidebarVisible = !mobilePreferences.sidebarVisible
            onToggleBottomRequested: mobilePreferences.bottomPanelVisible = !mobilePreferences.bottomPanelVisible
            onAudioSettingsRequested: audioSettingsDialog.open()
            onPreviewSettingsRequested: previewSettingsDialog.open()
        }
        RowLayout {
            id: workspace
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            ActivityBar {
                id: activityBar
                Layout.fillHeight: true
                Layout.preferredWidth: implicitWidth
                activeView: window.activePage === "export" ? "export" : "chart"
                normalizationEnabled: false
                onViewRequested: view => window.activePage = view
                onToolRequested: tool => assetsDialog.open()
                onSettingsRequested: previewSettingsDialog.open()
            }
            Item {
                id: workspaceHost
                objectName: "mobileWorkspaceHost"
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property int orientation: Qt.Horizontal
                Rectangle {
                    id: sidebarContent
                    objectName: "mobileSidebarContent"
                    visible: window.sidebarEffectivelyVisible
                    width: visible ? (window.sidebarResizing ? window.sidebarDragWidth : mobilePreferences.sidebarWidth) : 0
                    height: parent.height
                    color: Theme.surfaceColor(Theme.colors.background.panel)
                    ChartFieldSidebar {
                        anchors.fill: parent
                        visible: window.activePage === "chart"
                        viewState: state
                        documentSession: androidSession
                        commands: commands
                        pages: pages
                    }
                    ExportSidebarPage { anchors.fill: parent; visible: window.activePage === "export"; pages: pages; documentAvailable: androidSession.hasDocument }
                }
                SplitView {
                    id: workspaceSplit
                    objectName: "mobileWorkspaceSplit"
                    x: sidebarContent.width + sidebarHandle.width
                    width: parent.width - x
                    height: parent.height
                    orientation: Qt.Horizontal
                    property real previewWidthAtPress: 0
                    property bool previewDragStarted: false
                    onResizingChanged: {
                        if (resizing) {
                            previewWidthAtPress = previewPane.width
                            previewDragStarted = true
                        } else if (previewDragStarted) {
                            window.persistPreviewWidthRatio()
                            previewDragStarted = false
                        }
                    }
                    handle: SplitHandle {
                        objectName: "mobilePreviewDivider"
                        Accessible.role: Accessible.Splitter
                        Accessible.name: qsTrId("dialog.render_settings.preview_group")
                        handlePressed: previewPointerArea.pressed
                        handleHovered: previewPointerArea.containsMouse
                    }
                    SplitView {
                        id: centerSplit
                        objectName: "mobileCenterSplit"
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: Math.max(bottomPanel.minimumWidth,
                            Math.min(window.previewEditorAvailableWidth * (1 - mobilePreferences.previewMaximumWidthRatio),
                                window.previewEditorAvailableWidth - previewPane.minimumWidth))
                        orientation: Qt.Vertical
                        property real panelHeightAtPress: 0
                        onResizingChanged: if (resizing) panelHeightAtPress = bottomPanel.height
                        handle: SplitHandle {
                            objectName: "mobileBottomDivider"
                            Accessible.role: Accessible.Splitter
                            Accessible.name: mobileTimeline.timelineTabLabel
                            onReleased: window.persistBottomPanelHeightRatio()
                        }
                        Item {
                            id: editorHost
                            objectName: "mobileEditorHost"
                            SplitView.fillHeight: true
                            SplitView.minimumHeight: 180
                            EditorPane {
                                id: editorPane
                                anchors.fill: parent
                                visible: window.activePage === "chart"
                                viewState: state
                                documentSession: androidSession
                                commands: commands
                                editorController: v2EditorController
                                editorSync: mobileEditorSync
                                preferences: mobilePreferences
                                latency: mobileLatency
                                pages: pages
                                onOpenRequested: openProjectDialog.open()
                            }
                            ExportVideoPage {
                                anchors.fill: parent
                                visible: window.activePage === "export"
                                pages: pages
                                previewSession: mobilePreview
                                previewSettings: mobileExport.settings
                            }
                        }
                        BottomPanel {
                            id: bottomPanel
                            objectName: "mobileBottomPanel"
                            visible: window.bottomPanelEffectivelyVisible
                            SplitView.minimumHeight: visible ? Math.max(minimumHeight,
                                centerSplit.height * mobilePreferences.bottomPanelMinimumHeightRatio) : 0
                            SplitView.maximumHeight: visible ? Math.max(minimumHeight, Math.min(
                                centerSplit.height * mobilePreferences.bottomPanelMaximumHeightRatio,
                                centerSplit.height - editorHost.SplitView.minimumHeight - Theme.splitDividerThickness)) : 0
                            documentSession: androidSession
                            analysisSession: mobileAnalysis
                            preferences: mobilePreferences
                            timelineSession: mobileTimeline
                            previewSession: mobilePreview
                            onAnalysisRowActivated: (difficultyId, revision, line, column, endColumn, second) => {
                                if (!mobileAnalysis.completeRowActivation(difficultyId, revision, line, column, endColumn, second)) return
                                const start = androidSession.chartPosition(line, column)
                                const end = androidSession.chartPosition(line, endColumn)
                                mobileEditorSync.requestNavigation(difficultyId, revision, start, end, true, true)
                                if (second >= 0) mobilePreview.positionSeconds = second
                            }
                        }
                    }
                    PreviewPane {
                        id: previewPane
                        objectName: "v2PreviewPane"
                        minimumStageSize: minimumWidth
                        SplitView.minimumWidth: Math.max(minimumWidth,
                            Math.min(window.previewEditorAvailableWidth * mobilePreferences.previewMinimumWidthRatio,
                                window.previewEditorAvailableWidth - bottomPanel.minimumWidth))
                        SplitView.maximumWidth: Math.max(minimumWidth,
                            Math.min(window.previewEditorAvailableWidth * mobilePreferences.previewMaximumWidthRatio,
                                window.previewEditorAvailableWidth - bottomPanel.minimumWidth))
                        previewSession: mobilePreview
                        exportPageActive: window.activePage === "export"
                        preferences: mobilePreferences
                        rangePreviewState: mobileRangePreview
                        surfaceActive: !window.previewFullscreen
                        onFullscreenRequested: window.previewFullscreen = true
                    }
                }
                // Put the preview gesture band above both pane owners, as the
                // sidebar gesture is. The source ScrollView and preview surface
                // cannot consume a touch that begins on this shared boundary.
                Item {
                    width: Theme.splitHandleHitExtent
                    height: workspaceSplit.height
                    x: workspaceSplit.x + previewPane.x
                        + (window.previewOnLeft ? previewPane.width : -Theme.splitDividerThickness)
                        - (width - Theme.splitDividerThickness) / 2
                    enabled: !window.previewFullscreen
                    MouseArea {
                        id: previewPointerArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.SplitHCursor
                        preventStealing: true
                        property real startX: 0
                        property real startWidth: 0
                        property real desiredWidth: 0
                        function updateWidth(mouse) {
                            desiredWidth = Math.max(previewPane.SplitView.minimumWidth,
                                Math.min(previewPane.SplitView.maximumWidth,
                                    startWidth + (window.previewOnLeft ? 1 : -1)
                                        * (mapToItem(workspaceHost, mouse.x, mouse.y).x - startX)))
                            previewPane.SplitView.preferredWidth = desiredWidth
                        }
                        function finishDrag() {
                            if (Math.abs(desiredWidth - startWidth) > 0.01)
                                mobilePreferences.previewWidthRatio = desiredWidth / window.previewEditorAvailableWidth
                            window.previewPointerResizing = false
                        }
                        onPressed: mouse => {
                            startX = mapToItem(workspaceHost, mouse.x, mouse.y).x
                            startWidth = previewPane.width
                            desiredWidth = startWidth
                            window.previewPointerResizing = true
                        }
                        onPositionChanged: mouse => { if (pressed) updateWidth(mouse) }
                        onReleased: mouse => { updateWidth(mouse); finishDrag() }
                        onCanceled: finishDrag()
                    }
                }
                SplitHandle {
                    id: sidebarHandle
                    objectName: "mobileSidebarDivider"
                    Accessible.role: Accessible.Splitter
                    Accessible.name: qsTrId("qml.toggle_sidebar")
                    x: sidebarContent.width
                    width: window.sidebarEffectivelyVisible ? Theme.splitDividerThickness : 0
                    height: parent.height
                    showDivider: window.sidebarEffectivelyVisible
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
                            startX = mapToItem(workspaceHost, mouse.x, mouse.y).x
                            startWidth = window.showSidebar ? mobilePreferences.sidebarWidth : 0
                            window.sidebarDragWidth = mobilePreferences.sidebarWidth
                            window.sidebarDragVisible = window.showSidebar
                            window.sidebarResizing = true
                        }
                        onPositionChanged: mouse => {
                            if (pressed) window.resizeSidebar(startWidth + mapToItem(workspaceHost, mouse.x, mouse.y).x - startX)
                        }
                        onReleased: { window.persistSidebarWidth(); window.sidebarResizing = false }
                        onCanceled: { window.persistSidebarWidth(); window.sidebarResizing = false }
                    }
                }
            }
        }
        StatusBar {
            id: statusBar
            objectName: "mobileStatusBar"
            Layout.fillWidth: true
            documentName: statusNoticeTimer.running && statusMessage.text.length > 0
                ? statusMessage.text : androidSession.currentFilePath
            cursorLine: state.editorCursorLine
            cursorColumn: state.editorCursorColumn
            difficultyActive: state.difficultyEditorActive
            selectionBeatText: editorPane.selectionBeatStatusText
        }
    }
    QtObject {
        id: statusMessage
        property string text: androidSession.status
        onTextChanged: statusNoticeTimer.restart()
    }
    Timer { id: statusNoticeTimer; interval: 5000 }
    // SplitView writes preferred dimensions while dragging. Reattach the
    // stored proportions after release so window size changes keep the layout.
    Binding {
        target: previewPane.SplitView
        property: "preferredWidth"
        value: window.previewEditorAvailableWidth * mobilePreferences.previewWidthRatio
        when: !workspaceSplit.resizing && !window.previewPointerResizing
        restoreMode: Binding.RestoreNone
    }
    Binding {
        target: bottomPanel.SplitView
        property: "preferredHeight"
        value: window.bottomPanelEffectivelyVisible ? centerSplit.height * mobilePreferences.bottomPanelHeightRatio : 0
        when: !centerSplit.resizing
        restoreMode: Binding.RestoreNone
    }
    // The cover workspace follows the workbench in paint order. Its geometry
    // and touch coordinates use the same logical-to-screen transform.
    Loader {
        id: coverLoader
        anchors.fill: parent
        active: window.coverOpen
        sourceComponent: CoverExportPage {
            objectName: "mobileCoverPage"
            coverSession: mobileCover.session
            batchExportController: mobileCover.batch
            onCloseRequested: window.coverOpen = false
        }
    }
    }
    // Same stage / transport composition as v2 MainSplitView fullscreen.
    Rectangle {
        anchors.fill: parent
        visible: window.previewFullscreen
        color: Theme.surfaceColor(Theme.colors.background.panel)
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: fullscreenTransport.top
            Loader {
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height * mobilePreview.canvasAspectRatio)
                height: width / mobilePreview.canvasAspectRatio
                active: window.previewFullscreen && width >= 64 && height >= 64
                sourceComponent: Preview.PreviewSurface {
                    runtime: mobilePreview.runtime
                    mediaHost: mobilePreview.mediaHost
                    logger: mobilePreview
                    surfaceRole: "fullscreen"
                    backgroundColor: "transparent"
                    hudTextColor: Theme.colors.previewHud.text
                    hudShadowColor: Theme.colors.previewHud.shadow
                }
            }
            PreviewRateToast { anchors.fill: parent; previewSession: mobilePreview }
            IconButton {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                iconSource: "qrc:/icons/fullscreen.svg"
                tooltip: qsTrId("qml.exit_fullscreen_preview")
                onClicked: window.previewFullscreen = false
            }
        }
        PreviewTransport {
            id: fullscreenTransport
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            previewSession: mobilePreview
            preferences: mobilePreferences
            rangePreviewState: mobileRangePreview
            showCanvasMenuButton: false
        }
    }
    }
    Shortcut { sequence: "Escape"; enabled: window.previewFullscreen; onActivated: window.previewFullscreen = false }
    AppMenu {
        id: fileMenu
        AppMenuItem { text: "新建"; onTriggered: confirmReplace("new") }
        AppMenuItem { text: "打开 maidata.txt"; onTriggered: confirmReplace("open") }
        AppMenuItem { text: "打开工程文件夹"; onTriggered: confirmReplace("folder") }
        AppMenuItem { text: "另存为"; onTriggered: androidSession.save(true) }
        AppMenuItem { text: "分享谱面"; onTriggered: androidSession.share() }
    }
    property string pendingReplace: ""
    ChoiceDialog {
        id: openProjectDialog
        title: "打开谱面"
        message: "工程文件夹可同时载入音频、背景与视频素材。"
        choices: [{ id: "folder", label: "工程文件夹" }, { id: "open", label: "maidata.txt" }, { id: "cancel", label: "取消" }]
        dismissChoiceId: "cancel"
        onChosen: id => { if (id !== "cancel") window.confirmReplace(id) }
    }
    function replaceProject() {
        if (pendingReplace === "new") androidSession.newProject()
        else if (pendingReplace === "folder") androidSession.openProjectFolder()
        else androidSession.openProject()
    }
    function confirmReplace(action) {
        pendingReplace = action
        if (androidSession.dirty) replaceDialog.open()
        else replaceProject()
    }
    ChoiceDialog {
        id: replaceDialog
        title: "切换工程"
        message: "当前谱面有未保存内容，继续切换？"
        choices: [{id:"cancel",label:"取消"},{id:"continue",label:"继续"}]
        dismissChoiceId: "cancel"
        onChosen: id => { if (id === "continue") { androidSession.flushRecovery(); window.replaceProject() } }
    }
    ChoiceDialog {
        id: recovery
        title: "恢复工程"
        message: "发现上次会话，是否恢复？"
        choices: [{id:"recover",label:"恢复"},{id:"new",label:"新建"}]
        dismissChoiceId: "recover"
        onChosen: id => { if (id === "recover") androidSession.recover(); else androidSession.newProject() }
    }
    AudioSettingsDialog {
        id: audioSettingsDialog
        objectName: "shellAudioSettingsDialog"
        audioSettings: mobileExport.audioSettings
    }
    PreviewSettingsDialog {
        id: previewSettingsDialog
        objectName: "shellPreviewSettingsDialog"
        previewSettings: mobileExport.settings
    }
    PreferencesDialog {
        id: settings
        objectName: "mobilePreferencesDialog"
        preferencesModel: mobilePreferencesModel
        preferences: mobilePreferences
        appBackground: mobileAppBackground
        shortcuts: mobileShortcuts
        updateService: mobileUpdates
        onUpdateRequested: updatePrompt.present()
        platformOptions: Component {
            AppSwitch {
                objectName: "mobileBackgroundExportSwitch"
                text: qsTrId("android.preferences.background_export")
                checked: androidSession.backgroundExportAllowed
                onToggled: androidSession.backgroundExportAllowed = checked
            }
        }
    }
    ShortcutBindings {
        enabled: !settings.visible
        shortcuts: mobileShortcuts
        commands: editorTools
        previewSession: mobilePreview
        preferencesModel: mobilePreferencesModel
        sourceEditorFocused: window.sourceEditorFocused
        chartCommandsEnabled: androidSession.hasDocument && !settings.visible
        playbackCommandsEnabled: androidSession.hasDocument && !settings.visible
        menuOwnsChartTransformShortcuts: false
        menuOwnsPreviewRateShortcuts: false
        onChartTransformRequested: opId => editorPane.applyChartTransform(opId)
    }
    ChoiceDialog {
        id: updatePrompt
        objectName: "shellUpdateAvailableDialog"
        function present() {
            if (!mobileUpdates.updateAvailable) return
            const detail = mobileUpdates.availableDetail()
            title = qsTrId("dialog.update.title")
            message = qsTrId("dialog.update.message").arg(detail.version)
            let lines = []
            if (detail.releasedAt) lines.push(qsTrId("dialog.update.released").arg(detail.releasedAt))
            if (detail.sizeText) lines.push(qsTrId("dialog.update.size").arg(detail.sizeText))
            if (detail.notes) lines.push(detail.notes)
            details = lines.join("\n")
            choices = [
                { id: "download", label: qsTrId("dialog.update.download"), role: "accept", enabled: !!detail.releasePageUrl },
                { id: "later", label: qsTrId("action.later"), role: "reject" },
                { id: "skip", label: qsTrId("dialog.update.skip"), role: "reject" }
            ]
            dismissChoiceId = "later"
            open()
        }
        onChosen: choiceId => {
            if (choiceId === "download") mobileUpdates.openDownloadPage()
            else if (choiceId === "skip") mobileUpdates.skipAvailableVersion()
        }
    }
    AppDialog {
        id: assetsDialog
        objectName: "mobileAssetsDialog"
        title: qsTrId("android.assets.title")
        preferredWidth: 700
        preferredHeight: Theme.dialogHeight
        body: ColumnLayout {
            spacing: 10
            RowLayout { AppButton { text: qsTrId("android.assets.audio"); onClicked: androidSession.importAsset("audio") } AppButton { text: qsTrId("android.assets.image"); onClicked: androidSession.importAsset("image") } }
            RowLayout { AppButton { text: qsTrId("android.assets.video"); onClicked: androidSession.importAsset("video") } AppButton { text: qsTrId("android.assets.font"); onClicked: androidSession.importAsset("font") } }
        }
        footer: DialogFooter { cancelText: qsTrId("action.close"); onRejected: assetsDialog.close() }
    }
    UiRequestHost { requests: mobileExport.requests; externalFileDialogs: Qt.platform.os === "android" }
    JobProgressOverlay { progress: mobileExport.progress }
}
