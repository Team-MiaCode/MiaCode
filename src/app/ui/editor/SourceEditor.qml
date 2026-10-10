import QtQuick
import QtQml.Models
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

Rectangle {
    id: root
    required property var viewState
    required property var documentSession
    required property var editorController
    required property var syncController
    required property var analysisSession
    property var preferences: null
    readonly property bool overviewRulerEnabled: preferences ? preferences.editorOverviewRulerEnabled : true
    property bool navigationVisible: false
    onNavigationVisibleChanged: {
        if (!navigationVisible) {
            editorContextMenu.close()
            bookmarkMenu.close()
        }
    }
    property int pendingBookmarkLine: -1
    readonly property int editorRailBaseWidth: 12
    readonly property real editorRailDevicePixelRatio: Math.max(0.01,
        root.Window.window ? root.Window.window.devicePixelRatio : 1)
    readonly property int editorRailPhysicalWidth: 3 * Math.ceil(
        editorRailBaseWidth * editorRailDevicePixelRatio / 3)
    readonly property real editorRailWidth: editorRailPhysicalWidth / editorRailDevicePixelRatio
    readonly property int editorRailLanePhysicalWidth: editorRailPhysicalWidth / 3
    readonly property real editorRailLaneWidth: editorRailLanePhysicalWidth / editorRailDevicePixelRatio
    readonly property int editorRailDisplayLineCount: Math.max(
        1, sourceArea.vertical_scroll_max + sourceArea.vertical_scroll_page)
    readonly property real editorRailCurrentMarkerHeight: Math.min(
        Math.ceil(2 * editorRailDevicePixelRatio) / editorRailDevicePixelRatio,
        verticalRailBackground.height)
    property int editorRailSceneRevision: 0
    readonly property real editorRailSceneX: {
        // mapToItem() needs an explicit dependency when an ancestor moves.
        const revision = editorRailSceneRevision
        return root.mapToItem(null, 0, 0).x
    }
    readonly property real editorRailRightMargin:
        width - alignRailCoordinate(width, editorRailSceneX)
    signal normalizeChartRequested()
    readonly property var bookmarks: sourceArea.bookmarks
    readonly property bool canUndo: sourceArea.canUndo
    readonly property bool canRedo: sourceArea.canRedo
    readonly property bool canCut: !sourceArea.readonly && sourceArea.selectionStart !== sourceArea.selectionEnd
    readonly property bool canCopy: sourceArea.selectionStart !== sourceArea.selectionEnd
    readonly property bool canPaste: !sourceArea.readonly && sourceArea.canPaste
    readonly property bool canTransform: canCut
    readonly property bool canNormalize: !sourceArea.readonly && documentSession.currentDifficultyId > 0
    readonly property var selectionBeatSummary: preferences && preferences.editorSelectionBeatDisplay
        ? (sourceArea.selectionStart !== sourceArea.selectionEnd
            ? documentSession.selectionBeatSummary(sourceArea.text, sourceArea.selectionStart, sourceArea.selectionEnd)
            : ({ totalCommaCount: 0, parts: [], exact: true }))
        : ({ totalCommaCount: 0, parts: [], exact: true })
    function beatSummaryDetail() {
        return (selectionBeatSummary.parts || []).map(part => part.count + "/" + part.denominator).join(" + ")
    }
    readonly property string selectionBeatStatusText: {
        const total = Number(selectionBeatSummary.totalCommaCount || 0)
        if (total <= 0) return ""
        const detail = beatSummaryDetail()
        const value = (selectionBeatSummary.parts || []).length > 1 ? total + " (" + detail + ")" : detail
        return qsTrId("document.selection_beats").arg(selectionBeatSummary.exact ? value : "~ " + value)
    }
    readonly property string selectionBeatTooltipText: selectionBeatStatusText.length === 0 ? ""
        : selectionBeatSummary.exact ? beatSummaryDetail() : qsTrId("document.selection_beats_inexact").arg(beatSummaryDetail())

    function alignRailCoordinate(value, sceneOrigin) {
        const ratio = editorRailDevicePixelRatio
        return Math.round((sceneOrigin + value) * ratio) / ratio - sceneOrigin
    }

    function overviewMarkerTop(marker) {
        const lineCount = root.editorRailDisplayLineCount
        const trackHeight = verticalRailBackground.height
        if (lineCount <= 0 || trackHeight <= 0) return 0
        const minimumHeight = Math.min(Math.ceil(2 * editorRailDevicePixelRatio)
                                      / editorRailDevicePixelRatio, trackHeight)
        const sceneTop = verticalRailBackground.sceneTop
        const top = alignRailCoordinate(trackHeight * marker.startDisplayLine / lineCount, sceneTop)
        return Math.max(0, Math.min(trackHeight - minimumHeight, top))
    }

    function overviewMarkerHeight(marker) {
        const lineCount = root.editorRailDisplayLineCount
        const trackHeight = verticalRailBackground.height
        if (lineCount <= 0 || trackHeight <= 0) return 0
        const minimumHeight = Math.min(Math.ceil(2 * editorRailDevicePixelRatio)
                                      / editorRailDevicePixelRatio, trackHeight)
        const top = overviewMarkerTop(marker)
        const sceneTop = verticalRailBackground.sceneTop
        const endDisplayLine = Math.max(marker.startDisplayLine, marker.endDisplayLine)
        const end = alignRailCoordinate(trackHeight * (endDisplayLine + 1) / lineCount, sceneTop)
        return Math.max(0, Math.min(trackHeight, Math.max(end, top + minimumHeight)) - top)
    }

    function overviewCurrentMarkerTop() {
        const lineCount = root.editorRailDisplayLineCount
        const displayLine = sourceArea.overviewCurrentDisplayLine
        const trackHeight = verticalRailBackground.height
        if (lineCount <= 0 || displayLine < 0 || trackHeight <= 0) return 0
        const markerHeight = editorRailCurrentMarkerHeight
        const sceneTop = verticalRailBackground.sceneTop
        const center = trackHeight * (displayLine + 0.5) / lineCount
        const top = alignRailCoordinate(center - markerHeight / 2, sceneTop)
        return Math.max(0, Math.min(trackHeight - markerHeight, top))
    }

    function overviewLaneColor(lane) {
        if (lane === 0) return Theme.colors.syntax.warning
        if (lane === 1) return Theme.colors.accent.primary
        return Theme.colors.syntax.error
    }
    color: Theme.surfaceColor(Theme.colors.background.surface)
    clip: true
    function undo() { sourceArea.undo() }
    function redo() { sourceArea.redo() }
    function cut() { sourceArea.cut() }
    function copy() { sourceArea.copy() }
    function paste() { sourceArea.paste() }
    function selectAll() { sourceArea.selectAll() }
    function selectCurrentLine() { sourceArea.selectCurrentLine() }
    function jumpToLine(line) { sourceArea.jumpToLine(line) }
    function openFindReplace() { findReplaceBar.show() }
    function exportSelectionRange() { sourceArea.exportSelectionRange() }
    function applyChartTransform(operation) { return sourceArea.applyChartTransform(operation) }
    function applyNormalization(options) { return sourceArea.applyNormalization(options) }
    function selectionDescription() {
        if (sourceArea.selectionStart === sourceArea.selectionEnd)
            return qsTrId("qml.normalize_the_entire_chart_source")
        const startLine = sourceArea.lineAtPosition(sourceArea.selectionStart)
        const endLine = sourceArea.lineAtPosition(sourceArea.selectionEnd - 1)
        return qsTrId("qml.normalize_selected_lines_1_2").arg(startLine).arg(endLine)
    }
    function createBookmarkAtLine(line) { return sourceArea.createBookmarkAtLine(line, qsTrId("qml.bookmarks")) }
    function deleteBookmarkAtLine(line) { return sourceArea.deleteBookmarkAtLine(line) }
    function renameBookmarkAtLine(line, title) { return sourceArea.renameBookmarkAtLine(line, title) }
    function promptRenameBookmark(line) {
        const bookmark = bookmarks.find(item => item.line === line)
        if (!bookmark) return
        pendingBookmarkLine = line
        bookmarkTitleField.text = bookmark.title
        bookmarkTitleDialog.open()
    }
    function openContextMenuAt(x, y) {
        sourceArea.forceActiveFocus()
        const position = sourceArea.positionAt(x, y)
        const line = sourceArea.textPositionRectangle(position)
        editorContextMenu.anchorPosition = position
        editorContextMenu.anchorOffset = Qt.point(x - line.x, y - line.y)
        editorContextMenu.prepareItems()
        editorContextMenu.updatePlacement()
        editorContextMenu.open()
    }
    function updateMenuPlacement() {
        if (editorContextMenu.visible || bookmarkMenu.visible)
            Qt.callLater(root.placeMenus)
    }
    function placeMenus() {
        if (editorContextMenu.visible) editorContextMenu.updatePlacement()
        if (bookmarkMenu.visible) bookmarkMenu.updatePlacement()
    }
    AppDialog {
        id: bookmarkTitleDialog

        title: qsTrId("editor.bookmark.rename")
        footer: DialogFooter {
            acceptText: qsTrId("action.ok")
            cancelText: qsTrId("action.cancel")
            onAccepted: bookmarkTitleDialog.accept()
            onRejected: bookmarkTitleDialog.reject()
        }
        onAccepted: root.renameBookmarkAtLine(root.pendingBookmarkLine, bookmarkTitleField.text)
        body: AppTextField {
            id: bookmarkTitleField
            Accessible.name: qsTrId("qml.bookmark_name")
        }
    }

    AppMenu {
        id: editorContextMenu
        objectName: "editorContextMenu"
        modal: true
        dim: false
        property string pendingOperation: ""
        property int contextDifficulty: -1
        property double contextRevision: 0
        property double contextGeneration: 0
        onClosed: {
            const operation = pendingOperation
            pendingOperation = ""
            if (operation.length > 0 && root.navigationVisible
                    && contextDifficulty === root.documentSession.currentDifficultyId
                    && contextRevision === root.documentSession.documentRevision
                    && contextGeneration === root.documentSession.documentOpenGeneration) {
                sourceArea.forceActiveFocus()
                executeOperation(operation)
            }
        }
        parent: Overlay.overlay

        property rect placement: Qt.rect(0, 0, 0, 0)
        property int anchorPosition: 0
        property point anchorOffset: Qt.point(0, 0)
        x: placement.x
        y: placement.y
        width: placement.width
        height: placement.height

        function updatePlacement() {
            const line = sourceArea.textPositionRectangle(anchorPosition)
            placeAt(sourceArea.mapToItem(parent, line.x + anchorOffset.x, line.y + anchorOffset.y),
                    sourceArea.mapToItem(parent, line))
        }

        function placeAt(point, lineBounds) {
            // 点击位置决定水平锚点，命中行决定上下边缘。
            contentItem.forceLayout()
            const viewportWidth = parent.width
            const viewportHeight = parent.height
            const menuWidth = Math.min(implicitWidth, viewportWidth)
            const menuHeight = Math.min(measuredHeight(), viewportHeight)
            const gap = Theme.menuPadding
            const rightX = point.x + gap
            const menuX = Math.max(0, Math.min(rightX, viewportWidth - menuWidth))
            const menuY = Math.max(0, Math.min(point.y, viewportHeight - menuHeight))
            if (rightX + menuWidth <= viewportWidth) {
                // 即使长菜单向上贴齐窗口，左边缘仍在点击位置右侧。
                placement = Qt.rect(menuX, menuY, menuWidth, menuHeight)
                return
            }

            // 右侧空间不足时保留菜单宽度，沿命中行的上下边缘放置。
            const top = Math.max(0, Math.min(lineBounds.y - gap, viewportHeight))
            const bottom = Math.max(0, Math.min(
                lineBounds.y + lineBounds.height + gap, viewportHeight))
            const belowSpace = viewportHeight - bottom
            const aboveSpace = top
            if (belowSpace >= menuHeight) {
                placement = Qt.rect(menuX, bottom, menuWidth, menuHeight)
            } else if (aboveSpace >= menuHeight) {
                placement = Qt.rect(menuX, top - menuHeight, menuWidth, menuHeight)
            } else if (belowSpace >= aboveSpace) {
                placement = Qt.rect(menuX, bottom, menuWidth, belowSpace)
            } else {
                placement = Qt.rect(menuX, 0, menuWidth, aboveSpace)
            }
        }

        readonly property var transformRows: root.documentSession.chartTransformMenu()
        // 每次打开时确定操作列表，条目与几何在关闭动画期间保持一致。
        property bool hasSelection: false
        property bool canTransform: false
        property bool canNormalize: false

        function prepareItems() {
            pendingOperation = ""
            contextDifficulty = root.documentSession.currentDifficultyId
            contextRevision = root.documentSession.documentRevision
            contextGeneration = root.documentSession.documentOpenGeneration
            const selected = root.canCopy
            const transform = root.canTransform
            const normalize = root.canNormalize
            if (menuEntries.count > 0 && selected === hasSelection
                    && transform === canTransform && normalize === canNormalize)
                return
            hasSelection = selected
            canTransform = transform
            canNormalize = normalize

            const rows = []
            function action(labelKey, operation) {
                rows.push({ kind: "action", labelKey: labelKey, operation: operation })
            }
            function separator() {
                rows.push({ kind: "separator", labelKey: "", operation: "" })
            }
            action("action.cut", "cut")
            action("action.copy", "copy")
            action("action.paste", "paste")
            separator()
            action("net.select_all", "select_all")
            action("qml.find_and_replace", "find")
            if (selected || normalize)
                separator()
            if (selected)
                action("qml.export_selection", "export")
            if (transform) {
                separator()
                for (const section of [0, 2]) {
                    if (section > 0)
                        separator()
                    for (const row of transformRows.filter(row => row.section === section))
                        action(row.labelKey, row.id)
                }
            }
            if (normalize)
                action("qml.normalize_whole_chart", "normalize")
            if (transform)
                rows.push({ kind: "submenu", labelKey: "action.transform.more", operation: "" })
            menuEntries.clear()
            for (const row of rows)
                menuEntries.append(row)
        }

        function triggerOperation(operation) {
            pendingOperation = operation
        }
        function executeOperation(operation) {
            switch (operation) {
            case "cut": root.cut(); break
            case "copy": root.copy(); break
            case "paste": root.paste(); break
            case "select_all": root.selectAll(); break
            case "find": root.openFindReplace(); break
            case "export": root.exportSelectionRange(); break
            case "normalize": root.normalizeChartRequested(); break
            default: root.applyChartTransform(operation); break
            }
        }

        ListModel { id: menuEntries }

        Instantiator {
            model: menuEntries
            delegate: DelegateChooser {
                role: "kind"
                DelegateChoice {
                    roleValue: "action"
                    delegate: AppMenuItem {
                        required property string labelKey
                        required property string operation
                        text: qsTrId(labelKey)
                        enabled: operation === "cut" ? root.canCut
                            : operation === "copy" ? root.canCopy
                            : operation === "paste" ? root.canPaste : true
                        onTriggered: editorContextMenu.triggerOperation(operation)
                    }
                }
                DelegateChoice {
                    roleValue: "separator"
                    delegate: AppMenuSeparator {}
                }
                DelegateChoice {
                    roleValue: "submenu"
                    delegate: AppMenu {
                        id: transformMoreMenu
                        title: qsTrId("action.transform.more")
                        readonly property var subdivisionRows: editorContextMenu.transformRows.filter(row => row.section === 1)
                        Instantiator {
                            model: transformMoreMenu.subdivisionRows
                            delegate: AppMenuItem {
                                required property var modelData
                                text: qsTrId(modelData.labelKey)
                                onTriggered: editorContextMenu.triggerOperation(modelData.id)
                            }
                            onObjectAdded: (index, item) => transformMoreMenu.insertItem(index, item)
                            onObjectRemoved: (index, item) => transformMoreMenu.removeItem(item)
                        }
                        AppMenuSeparator {}
                        Instantiator {
                            model: editorContextMenu.transformRows.filter(row => row.section === 3)
                            delegate: AppMenuItem {
                                required property var modelData
                                text: qsTrId(modelData.labelKey)
                                onTriggered: editorContextMenu.triggerOperation(modelData.id)
                            }
                            onObjectAdded: (index, item) => transformMoreMenu.insertItem(
                                transformMoreMenu.subdivisionRows.length + 1 + index, item)
                            onObjectRemoved: (index, item) => transformMoreMenu.removeItem(item)
                        }
                    }
                }
            }
            onObjectAdded: (index, item) => {
                if (item instanceof AppMenu)
                    editorContextMenu.insertMenu(index, item)
                else
                    editorContextMenu.insertItem(index, item)
            }
            onObjectRemoved: (index, item) => {
                if (item instanceof AppMenu)
                    editorContextMenu.removeMenu(item)
                else
                    editorContextMenu.removeItem(item)
            }
        }
    }


    FindReplaceBar {
        id: findReplaceBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        editor: sourceArea
    }
    ScintillaEditor {
        id: sourceArea
        objectName: "sourceArea"
        property bool reservesPlainSpace: true
        anchors.left: parent.left
        anchors.right: verticalBar.left
        anchors.top: findReplaceBar.bottom
        anchors.bottom: horizontalBar.top
        documentSession: root.documentSession
        controller: root.editorController
        syncController: root.syncController
        analysisSession: root.analysisSession
        navigationVisible: root.navigationVisible
        font: Theme.codeFont
        HoverHandler {
            cursorShape: Qt.IBeamCursor
        }
        blockSpacing: root.preferences ? root.preferences.editorBlockSpacing : 0
        autoWrap: root.preferences ? root.preferences.editorAutoWrap : true
        scrollPastEnd: root.preferences ? root.preferences.editorScrollPastEnd : true
        editorColors: ({ text: Theme.colors.text.editor,
                    background: Theme.surfaceColor(Theme.colors.background.surface),
                    lineNumber: Theme.colors.text.lineNumber, accent: Theme.colors.accent.primary,
                    followOpacity: Theme.followHighlightOpacity,
                    keyword: Theme.colors.syntax.keyword, duration: Theme.colors.syntax.duration,
                    comment: Theme.colors.syntax.comment, error: Theme.colors.syntax.error,
                    warning: Theme.colors.syntax.warning, follow: Theme.colors.state.followHighlight,
                    currentLine: Theme.overlayColor(Theme.colors.state.focusLine),
                    selection: Theme.overlayColor(Theme.colors.state.selectionHighlight) })
        Rectangle {
            x: sourceArea.followCursorRectangle.x
            y: sourceArea.followCursorRectangle.y
            width: 2
            height: sourceArea.followCursorRectangle.height
            color: Theme.colors.accent.primary
            visible: sourceArea.followCaretVisible
        }
        // Popup keyboard focus belongs to the menu; the editing location stays
        // visible until the popup closes and returns focus to Scintilla.
        Rectangle {
            x: sourceArea.cursorRectangle.x
            y: sourceArea.cursorRectangle.y
            width: 2
            height: sourceArea.cursorRectangle.height
            color: Theme.colors.text.editor
            visible: editorContextMenu.visible && !sourceArea.readonly
                     && !root.syncController.followPlaybackActive
        }
        onScenePositionChanged: root.editorRailSceneRevision += 1
        onSelectionChanged: {
            root.viewState.editorCursorLine = cursorLine
            root.viewState.editorCursorColumn = cursorColumn
        }
        onActiveFocusChanged: {
            if (!activeFocus && !completionPopup.pointerInside)
                root.editorController.closeCompletion()
        }
        onFindRequested: root.openFindReplace()
        onContextMenuRequested: (x, y) => root.openContextMenuAt(x, y)
        onBookmarkMenuRequested: (line, x, y) => {
            root.pendingBookmarkLine = line
            bookmarkMenu.contextDifficulty = root.documentSession.currentDifficultyId
            bookmarkMenu.contextRevision = root.documentSession.documentRevision
            bookmarkMenu.contextGeneration = root.documentSession.documentOpenGeneration
            // Native margin handling finishes by focusing the item. Open the
            // popup after that event so its keyboard focus stays with the menu.
            Qt.callLater(() => {
                if (root.navigationVisible && bookmarkMenu.matchesDocument()) {
                    bookmarkMenu.updatePlacement()
                    bookmarkMenu.open()
                }
            })
        }
    }
    AppScrollBar {
        id: verticalBar
        anchors.top: sourceArea.top
        anchors.bottom: sourceArea.bottom
        anchors.right: parent.right
        anchors.rightMargin: root.editorRailRightMargin
        width: root.editorRailWidth
        leftPadding: 0
        rightPadding: 0
        orientation: Qt.Vertical
        hoverEnabled: true
        readonly property int scrollValue: sourceArea.vertical_scroll_value
        onScrollValueChanged: scrollActivityTimer.restart()
        active: hovered || pressed || scrollActivityTimer.running
        Timer {
            id: scrollActivityTimer
            interval: 1200
        }
        onPressedChanged: if (pressed) sourceArea.beginViewportInteraction()
        size: sourceArea.vertical_scroll_page / root.editorRailDisplayLineCount
        position: sourceArea.vertical_scroll_value / root.editorRailDisplayLineCount
        onPositionChanged: if (pressed) sourceArea.scrollVertical(Math.round(position * root.editorRailDisplayLineCount))
        contentItem: Item {
            implicitWidth: root.editorRailWidth
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: root.editorRailWidth
                radius: width / 2
                opacity: verticalBar.active ? 0.5 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
                color: verticalBar.hovered || verticalBar.pressed
                       ? Theme.colors.scroll.handleHover
                       : Theme.colors.scroll.handle
                visible: verticalBar.size < 1
            }
        }
        background: Item {
            id: verticalRailBackground
            visible: root.overviewRulerEnabled
            readonly property int alignmentRevision: root.editorRailSceneRevision
            readonly property real sceneLeft: {
                const revision = alignmentRevision
                return parent.mapToItem(null, 0, 0).x
            }
            readonly property real sceneTop: {
                const revision = alignmentRevision
                return parent.mapToItem(null, 0, 0).y
            }
            x: root.alignRailCoordinate(0, sceneLeft)
            y: root.alignRailCoordinate(0, sceneTop)
            width: root.alignRailCoordinate(parent.width, sceneLeft) - x
            height: root.alignRailCoordinate(parent.height, sceneTop) - y

            Repeater {
                model: sourceArea.overviewMarkers
                delegate: Rectangle {
                    required property var modelData
                    x: modelData.lane * root.editorRailLaneWidth
                    y: root.overviewMarkerTop(modelData)
                    width: root.editorRailLaneWidth
                    height: root.overviewMarkerHeight(modelData)
                    color: root.overviewLaneColor(modelData.lane)
                }
            }
            Rectangle {
                x: 0
                y: root.overviewCurrentMarkerTop()
                width: root.editorRailWidth
                height: root.editorRailCurrentMarkerHeight
                color: sourceArea.overviewFollowing
                       ? Theme.colors.state.followHighlight
                       : Theme.contentOverlayColor(Theme.colors.text.editor, 0.55)
                visible: sourceArea.overviewCurrentDisplayLine >= 0
                         && sourceArea.overviewDisplayLineCount > 0
            }
        }
    }
    AppScrollBar {
        id: horizontalBar
        anchors.left: sourceArea.left
        anchors.right: verticalBar.left
        anchors.bottom: parent.bottom
        orientation: Qt.Horizontal
        visible: !sourceArea.autoWrap && sourceArea.horizontal_scroll_max > 0
        height: visible ? implicitHeight : 0
        hoverEnabled: true
        active: hovered || pressed || sourceArea.activeFocus
        onPressedChanged: if (pressed) sourceArea.beginViewportInteraction()
        size: sourceArea.horizontal_scroll_page / Math.max(1, sourceArea.horizontal_scroll_max + sourceArea.horizontal_scroll_page)
        position: sourceArea.horizontal_scroll_value / Math.max(1, sourceArea.horizontal_scroll_max + sourceArea.horizontal_scroll_page)
        onPositionChanged: if (pressed) sourceArea.scrollHorizontal(Math.round(position * (sourceArea.horizontal_scroll_max + sourceArea.horizontal_scroll_page)))
    }
    CompletionPopup {
        id: completionPopup
        editor: sourceArea
        controller: root.editorController
    }
    Binding {
        target: root.Window.window
        property: "sourceEditorFocused"
        value: sourceArea.activeFocus || editorContextMenu.visible || bookmarkMenu.visible
    }
    Binding {
        target: root.Window.window
        property: "sourceEditorOverlayHeld"
        value: completionPopup.pointerInside
    }
    AppMenu {
        id: bookmarkMenu
        parent: Overlay.overlay
        modal: true
        dim: false
        property string pendingOperation: ""
        property int contextDifficulty: -1
        property double contextRevision: 0
        property double contextGeneration: 0
        readonly property bool hasBookmark: root.bookmarks.some(item => item.line === root.pendingBookmarkLine)
        function matchesDocument() {
            return contextDifficulty === root.documentSession.currentDifficultyId
                && contextRevision === root.documentSession.documentRevision
                && contextGeneration === root.documentSession.documentOpenGeneration
        }
        function updatePlacement() {
            const line = sourceArea.lineRectangle(root.pendingBookmarkLine)
            const point = sourceArea.mapToItem(parent, 16, line.y)
            x = Math.max(0, Math.min(point.x, parent.width - width))
            y = Math.max(0, Math.min(point.y, parent.height - height))
        }
        onClosed: {
            const operation = pendingOperation
            pendingOperation = ""
            if (operation.length === 0 || !root.navigationVisible
                    || contextDifficulty !== root.documentSession.currentDifficultyId
                    || contextRevision !== root.documentSession.documentRevision
                    || contextGeneration !== root.documentSession.documentOpenGeneration)
                return
            sourceArea.forceActiveFocus()
            if (operation === "jump") root.jumpToLine(root.pendingBookmarkLine)
            else if (operation === "create") root.createBookmarkAtLine(root.pendingBookmarkLine)
            else if (operation === "rename") root.promptRenameBookmark(root.pendingBookmarkLine)
            else root.deleteBookmarkAtLine(root.pendingBookmarkLine)
        }
        AppMenuItem { text: qsTrId("qml.jump_to_this_line"); onTriggered: bookmarkMenu.pendingOperation = "jump" }
        AppMenuItem {
            text: bookmarkMenu.hasBookmark ? qsTrId("editor.bookmark.delete") : qsTrId("qml.create_bookmark")
            onTriggered: bookmarkMenu.pendingOperation = bookmarkMenu.hasBookmark ? "delete" : "create"
        }
        AppMenuItem {
            text: qsTrId("editor.bookmark.rename")
            enabled: bookmarkMenu.hasBookmark
            onTriggered: bookmarkMenu.pendingOperation = "rename"
        }
    }
    Connections {
        target: root.viewState
        function onEditorClosed(key) { sourceArea.dropDocument(key) }
    }
    Connections {
        target: sourceArea
        function onLayoutChanged() { root.updateMenuPlacement() }
        function onScenePositionChanged() { root.updateMenuPlacement() }
        function onCursorRectangleChanged() { root.updateMenuPlacement() }
    }
    Connections {
        target: editorContextMenu.parent
        enabled: editorContextMenu.visible || bookmarkMenu.visible
        function onWidthChanged() { root.updateMenuPlacement() }
        function onHeightChanged() { root.updateMenuPlacement() }
    }
    Connections {
        target: root.documentSession
        function onDocumentReplaced() {
            editorContextMenu.close()
            bookmarkMenu.close()
        }
        function onDocumentStateChanged() {
            if (editorContextMenu.visible
                    && (editorContextMenu.contextDifficulty !== root.documentSession.currentDifficultyId
                        || editorContextMenu.contextRevision !== root.documentSession.documentRevision
                        || editorContextMenu.contextGeneration !== root.documentSession.documentOpenGeneration))
                editorContextMenu.close()
            if (bookmarkMenu.visible && !bookmarkMenu.matchesDocument()) bookmarkMenu.close()
        }
    }
}
