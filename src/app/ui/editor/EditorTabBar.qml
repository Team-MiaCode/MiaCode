pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Shapes
import MiaCode.UI

Item {
    id: root

    required property var viewState
    required property var documentSession
    required property var commands
    required property var pages

    readonly property int minimumTabWidth: 80
    readonly property int tabCount: viewState.openEditorTabs.length
    readonly property real barHeight: implicitHeight
    readonly property real availableTabWidth: Math.max(0, width - (tabsOverflow ? overflowButton.width : 0))
    readonly property real activePreferredWidth: activeTabIndex >= 0
        ? preferredWidthForKey(viewState.activeEditorKey) : 0
    readonly property bool tabsOverflow: activeTabIndex >= 0
        ? activePreferredWidth + (tabCount - 1) * minimumTabWidth > width
        : tabCount * minimumTabWidth > width

    function preferredWidthForKey(key) {
        const title = (documentSession.dirtyEditorKeys.indexOf(key) >= 0 ? "*" : "") + titleForKey(key)
        const iconWidth = difficultyIdForKey(key) > 0 ? Theme.difficultySwatchSize
            : iconForKey(key).toString().length > 0 ? 15 : 0
        return Math.max(minimumTabWidth,
            Math.ceil(tabFontMetrics.advanceWidth(title) + 14 + 9 + 24 + 6 + (iconWidth > 0 ? iconWidth + 6 : 0)))
    }

    readonly property var preferredTabWidths: viewState.openEditorTabs.map(key => preferredWidthForKey(key))
    readonly property real preferredTabsWidth: preferredTabWidths.reduce((sum, value) => sum + value, 0)
    readonly property real inactiveTabScale: {
        const otherCount = tabCount - (activeTabIndex >= 0 ? 1 : 0)
        const otherPreferredWidth = preferredTabsWidth - activePreferredWidth
        const minimumOtherWidth = otherCount * minimumTabWidth
        const remainingWidth = availableTabWidth - activePreferredWidth
        return otherPreferredWidth > minimumOtherWidth
            ? Math.max(0, Math.min(1, (remainingWidth - minimumOtherWidth)
                / (otherPreferredWidth - minimumOtherWidth))) : 1
    }
    readonly property var tabWidths: preferredTabWidths.map((value, index) => index === activeTabIndex
        ? value : minimumTabWidth + (value - minimumTabWidth) * inactiveTabScale)
    readonly property real totalTabsWidth: tabWidths.reduce((sum, value) => sum + value, 0)

    // Drag reordering state (live preview without mutating openEditorTabs mid-drag).
    property string draggingEditorKey: ""
    property int dragStartIndex: -1
    property int dragTargetIndex: -1
    property real dragCurrentX: 0
    property real dragWidth: 0
    property real dragPointerOffsetX: 0
    property real dragPointerViewportX: 0
    property bool animateDropSlot: false
    readonly property bool isDragging: draggingEditorKey.length > 0
        && dragStartIndex >= 0 && dragStartIndex < tabCount
    readonly property bool draggingActiveTab: isDragging
        && draggingEditorKey === viewState.activeEditorKey

    property string suppressClickKey: ""

    // Keep layout transitions ready after mount, suppressing only during live bar resize.
    property bool animationsReady: false
    property bool suppressAnimations: false
    property bool suppressHorizontalAnim: false
    property int previousActiveIndex: -1
    readonly property bool transitionsEnabled: Theme.animationsEnabled && animationsReady && !suppressAnimations

    // Remember the last active tab bounds so the arch can retract in place when closing the last tab.
    property real retainedActiveLeft: 0
    property real retainedActiveRight: 0
    property bool retainedActiveAtLeftEdge: true
    property bool retainedActiveAtRightEdge: false

    ListModel {
        id: tabModel
    }

    readonly property var visualOrder: {
        const order = []
        for (let i = 0; i < tabCount; ++i)
            order.push(i)
        if (isDragging) {
            const clamped = Math.max(0, Math.min(tabCount - 1, dragTargetIndex))
            order.splice(dragStartIndex, 1)
            order.splice(clamped, 0, dragStartIndex)
        }
        return order
    }

    readonly property var tabSlotPositions: {
        const positions = new Array(tabCount)
        let x = 0
        for (let slot = 0; slot < visualOrder.length; ++slot) {
            const origIndex = visualOrder[slot]
            positions[origIndex] = x
            x += (origIndex === dragStartIndex && isDragging)
                ? dragWidth : (tabWidths[origIndex] || 0)
        }
        return positions
    }

    readonly property real dragTargetSlotX: isDragging ? (tabSlotPositions[dragStartIndex] || 0) : 0

    function computeDragTargetIndex(currentX) {
        if (!isDragging || tabCount <= 1)
            return dragStartIndex
        let accX = 0
        let slot = 0
        for (let i = 0; i < tabCount; ++i) {
            if (i === dragStartIndex)
                continue
            const w = tabWidths[i] || minimumTabWidth
            if (currentX > accX + w * 0.5)
                slot += 1
            accX += w
        }
        return Math.max(0, Math.min(tabCount - 1, slot))
    }

    function updateDragPosition(sceneX, sceneY) {
        if (!isDragging)
            return
        const inViewport = tabViewport.mapFromItem(null, sceneX, sceneY)
        dragPointerViewportX = inViewport.x
        const inRow = tabRow.mapFromItem(null, sceneX, sceneY)
        const maxX = Math.max(0, totalTabsWidth - dragWidth)
        dragCurrentX = Math.max(0, Math.min(maxX, inRow.x - dragPointerOffsetX))
        dragTargetIndex = computeDragTargetIndex(dragCurrentX)
    }

    function cancelDrag() {
        animateDropSlot = false
        draggingEditorKey = ""
        dragStartIndex = -1
        dragTargetIndex = -1
        dragWidth = 0
    }

    function snapHorizontalContour() {
        suppressHorizontalAnim = true
        animatedActiveLeft = Qt.binding(() => root.targetActiveLeft)
        animatedActiveRight = Qt.binding(() => root.targetActiveRight)
        animatedLeftCurve = Qt.binding(() => root.effectiveAtLeftEdge ? 0 : root.curve)
        animatedRightCurve = Qt.binding(() => root.effectiveAtRightEdge ? 0 : root.curve)
        suppressHorizontalAnim = false
    }

    function snapAnimatedLayout() {
        suppressAnimations = true
        scrollRevealAnim.stop()
        animatedActiveLeft = Qt.binding(() => root.targetActiveLeft)
        animatedActiveRight = Qt.binding(() => root.targetActiveRight)
        animatedLeftCurve = Qt.binding(() => root.effectiveAtLeftEdge ? 0 : root.curve)
        animatedRightCurve = Qt.binding(() => root.effectiveAtRightEdge ? 0 : root.curve)
        activeContourPresence = Qt.binding(() => root.activeTabIndex >= 0 ? 1 : 0)
        resizeResumeTimer.restart()
    }

    function updateRetainedActiveBounds() {
        if (activeTabIndex < 0)
            return
        const left = draggingActiveTab ? dragCurrentX : (tabSlotPositions[activeTabIndex] || 0)
        const right = draggingActiveTab ? (dragCurrentX + dragWidth) : (left + (tabWidths[activeTabIndex] || 0))
        if (right > left) {
            retainedActiveLeft = left
            retainedActiveRight = right
            retainedActiveAtLeftEdge = (left - tabViewport.contentX) <= outlineWidth
            retainedActiveAtRightEdge = (right - tabViewport.contentX) >= width - outlineWidth
        }
    }

    function syncTabModel() {
        const nextKeys = root.viewState.openEditorTabs || []
        const oldLiveKeys = []
        for (let i = 0; i < tabModel.count; ++i) {
            const row = tabModel.get(i)
            if (!row.closing)
                oldLiveKeys.push(row.tabKey)
        }

        let hasOverlap = false
        for (let i = 0; i < nextKeys.length; ++i) {
            if (oldLiveKeys.indexOf(nextKeys[i]) >= 0) {
                hasOverlap = true
                break
            }
        }
        const fullReplacement = oldLiveKeys.length > 0 && nextKeys.length > 0 && !hasOverlap
        if (!root.animationsReady || fullReplacement) {
            tabModel.clear()
            for (let i = 0; i < nextKeys.length; ++i) {
                const k = nextKeys[i]
                tabModel.append({
                    tabKey: k,
                    closing: false,
                    closingX: 0,
                    closingWidth: 0,
                    collapseWidthOnClose: true,
                    cachedTitle: titleForKey(k),
                    cachedIcon: iconForKey(k).toString(),
                    cachedFilledIcon: iconForKey(k, true).toString(),
                    cachedDifficultyId: difficultyIdForKey(k),
                    animateEnter: false
                })
            }
            if (root.animationsReady && fullReplacement)
                snapAnimatedLayout()
            return
        }

        // 1. Mark removed live tabs as closing while keeping their last visual geometry.
        const collapseOnClose = nextKeys.length > 0
        for (let i = 0; i < tabModel.count; ++i) {
            const row = tabModel.get(i)
            if (!row.closing && nextKeys.indexOf(row.tabKey) < 0) {
                const item = tabRepeater.itemAt(i)
                const curX = item ? item.x : 0
                const curW = item ? item.width : minimumTabWidth
                const dirty = documentSession.dirtyEditorKeys.indexOf(row.tabKey) >= 0
                tabModel.setProperty(i, "closingX", curX)
                tabModel.setProperty(i, "closingWidth", curW)
                tabModel.setProperty(i, "collapseWidthOnClose", collapseOnClose)
                tabModel.setProperty(i, "cachedTitle", (dirty ? "*" : "") + titleForKey(row.tabKey))
                tabModel.setProperty(i, "cachedIcon", iconForKey(row.tabKey).toString())
                tabModel.setProperty(i, "cachedFilledIcon", iconForKey(row.tabKey, true).toString())
                tabModel.setProperty(i, "cachedDifficultyId", difficultyIdForKey(row.tabKey))
                tabModel.setProperty(i, "closing", true)
            }
        }

        // 2. Reorder existing delegates in place and insert newly opened tabs.
        let modelCursor = 0
        for (let targetIdx = 0; targetIdx < nextKeys.length; ++targetIdx) {
            const k = nextKeys[targetIdx]
            let foundIdx = -1
            for (let j = 0; j < tabModel.count; ++j) {
                if (tabModel.get(j).tabKey === k) {
                    foundIdx = j
                    break
                }
            }
            if (foundIdx >= 0) {
                if (tabModel.get(foundIdx).closing)
                    tabModel.setProperty(foundIdx, "closing", false)
                while (modelCursor < tabModel.count
                        && tabModel.get(modelCursor).closing
                        && modelCursor !== foundIdx) {
                    modelCursor += 1
                }
                if (foundIdx !== modelCursor)
                    tabModel.move(foundIdx, modelCursor, 1)
                modelCursor += 1
            } else {
                while (modelCursor < tabModel.count && tabModel.get(modelCursor).closing)
                    modelCursor += 1
                tabModel.insert(modelCursor, {
                    tabKey: k,
                    closing: false,
                    closingX: 0,
                    closingWidth: 0,
                    collapseWidthOnClose: true,
                    cachedTitle: titleForKey(k),
                    cachedIcon: iconForKey(k).toString(),
                    cachedFilledIcon: iconForKey(k, true).toString(),
                    cachedDifficultyId: difficultyIdForKey(k),
                    animateEnter: root.transitionsEnabled
                })
                modelCursor += 1
            }
        }
    }

    function finalizeClosingTab(key) {
        for (let i = tabModel.count - 1; i >= 0; --i) {
            const row = tabModel.get(i)
            if (row.tabKey === key && row.closing) {
                tabModel.remove(i)
                return
            }
        }
    }

    function shouldShowSeparator(liveIndex, key, isClosing) {
        if (isClosing || liveIndex < 0)
            return false
        const visualSlot = visualOrder.indexOf(liveIndex)
        if (visualSlot < 0 || visualSlot >= visualOrder.length - 1)
            return false
        if (key === viewState.activeEditorKey || key === draggingEditorKey)
            return false
        const nextOrigIndex = visualOrder[visualSlot + 1]
        const nextKey = viewState.openEditorTabs[nextOrigIndex]
        return nextKey !== viewState.activeEditorKey && nextKey !== draggingEditorKey
    }

    FontMetrics {
        id: tabFontMetrics
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
    }

    onWidthChanged: {
        if (animationsReady)
            snapAnimatedLayout()
    }

    onTabWidthsChanged: {
        updateRetainedActiveBounds()
        revealTimer.restart()
    }

    onDragCurrentXChanged: {
        if (draggingActiveTab)
            updateRetainedActiveBounds()
    }

    Timer {
        id: revealTimer
        interval: 0
        onTriggered: root.revealActiveTab()
    }

    Timer {
        id: resizeResumeTimer
        interval: 16
        onTriggered: root.suppressAnimations = false
    }

    NumberAnimation {
        id: scrollRevealAnim
        target: tabViewport
        property: "contentX"
        duration: Theme.tabSlideDuration
        easing.type: Easing.OutCubic
    }

    Timer {
        id: dragAutoScrollTimer
        interval: 16
        repeat: true
        running: root.isDragging && root.tabsOverflow
        onTriggered: {
            const edgeZone = 32
            const maxScroll = Math.max(0, tabViewport.contentWidth - tabViewport.width)
            let delta = 0
            if (root.dragPointerViewportX < edgeZone && tabViewport.contentX > 0) {
                delta = -Math.max(2, Math.ceil((edgeZone - root.dragPointerViewportX) * 0.25))
            } else if (root.dragPointerViewportX > tabViewport.width - edgeZone && tabViewport.contentX < maxScroll) {
                delta = Math.max(2, Math.ceil((root.dragPointerViewportX - (tabViewport.width - edgeZone)) * 0.25))
            }
            if (delta === 0)
                return
            const nextContentX = Math.max(0, Math.min(maxScroll, tabViewport.contentX + delta))
            const applied = nextContentX - tabViewport.contentX
            if (Math.abs(applied) < 0.5)
                return
            tabViewport.contentX = nextContentX
            const maxX = Math.max(0, root.totalTabsWidth - root.dragWidth)
            root.dragCurrentX = Math.max(0, Math.min(maxX, root.dragCurrentX + applied))
            root.dragTargetIndex = root.computeDragTargetIndex(root.dragCurrentX)
        }
    }

    function difficultyData(id) {
        const difficulties = root.documentSession.difficulties
        for (let index = 0; index < difficulties.length; ++index) {
            if (difficulties[index].id === id)
                return difficulties[index]
        }
        return null
    }

    // Closing a dirty editor asks about that editor's staged content.
    function requestCloseTab(key) {
        const difficultyId = root.difficultyIdForKey(key)
        if (key === root.viewState.metadataEditorKey && !root.documentSession.finishPendingEdits())
            return
        if (key === root.viewState.metadataEditorKey || key === root.viewState.latencyEditorKey
                || root.viewState.isNetEditor(key)) {
            root.viewState.closeEditor(key)
            return
        }
        root.documentSession.requestCloseDifficulty(difficultyId)
    }

    function difficultyIdForKey(key) {
        return key.startsWith("difficulty:")
            ? Number(key.substring("difficulty:".length))
            : 0
    }

    function titleForKey(key) {
        if (key === viewState.netUploadEditorKey)
            return qsTrId("net.ui.upload_page")
        if (key === viewState.netDownloadEditorKey)
            return qsTrId("net.ui.download_page")
        if (key === viewState.metadataEditorKey)
            return qsTrId("dialog.unsaved_field_changes.field.metadata")
        if (key === viewState.latencyEditorKey)
            return qsTrId("qml.latency_calibration")
        const difficulty = difficultyData(difficultyIdForKey(key))
        return difficulty ? difficulty.label : qsTrId("dialog.batch_export.difficulty")
    }

    function iconForKey(key, filled = false) {
        const suffix = filled ? "-fill.svg" : ".svg"
        if (key === viewState.netDownloadEditorKey)
            return Qt.resolvedUrl("icons/download" + suffix)
        if (key === viewState.netUploadEditorKey)
            return Qt.resolvedUrl("icons/upload" + suffix)
        if (key === viewState.metadataEditorKey)
            return Qt.resolvedUrl("icons/metadata" + suffix)
        if (key === viewState.latencyEditorKey)
            return Qt.resolvedUrl("icons/metronome" + suffix)
        return ""
    }

    function tooltipForKey(key) {
        if (key === viewState.metadataEditorKey || key === viewState.latencyEditorKey)
            return ""
        const difficulty = difficultyData(difficultyIdForKey(key))
        if (!difficulty)
            return ""
        const fileIdentity = root.documentSession.currentFilePath.length > 0
            ? root.documentSession.currentFilePath
            : root.documentSession.currentFileName
        let result = fileIdentity + "\n" + difficulty.label
        if (difficulty.designer.length > 0)
            result += "\n" + qsTrId("qml.chart_designer_1").arg(difficulty.designer)
        return result
    }

    function activateTab(key) {
        if (root.viewState.isNetEditor(key)) {
            root.pages.openNetPage(key)
            return
        }
        if (key === viewState.latencyEditorKey) {
            root.pages.openLatencyPage()
            return
        }
        if (key === viewState.metadataEditorKey
                && (root.pages.activePageId === "latency" || root.pages.activePageId.startsWith("net-"))
                && !root.pages.activateMetadataPage())
            return
        viewState.activateEditor(key)
    }

    function revealActiveTab() {
        if (isDragging)
            return
        const index = activeTabIndex
        if (index < 0 || index >= tabCount)
            return
        const targetLeft = tabSlotPositions[index] || 0
        const targetWidth = tabWidths[index] || 0
        const maxScroll = Math.max(0, totalTabsWidth - tabViewport.width)
        let targetScroll = tabViewport.contentX
        if (targetLeft < tabViewport.contentX)
            targetScroll = targetLeft
        else if (targetLeft + targetWidth > tabViewport.contentX + tabViewport.width)
            targetScroll = targetLeft + targetWidth - tabViewport.width
        targetScroll = Math.max(0, Math.min(maxScroll, targetScroll))
        if (Math.abs(targetScroll - tabViewport.contentX) < 0.5)
            return
        if (root.transitionsEnabled && root.visible) {
            scrollRevealAnim.stop()
            scrollRevealAnim.from = tabViewport.contentX
            scrollRevealAnim.to = targetScroll
            scrollRevealAnim.start()
        } else {
            scrollRevealAnim.stop()
            tabViewport.contentX = targetScroll
        }
    }

    readonly property real outlineWidth: 1 / Screen.devicePixelRatio
    readonly property int activeTabIndex: viewState.openEditorTabs.indexOf(viewState.activeEditorKey)

    // Strip-space coordinates of the active tab so scrolling contentX never lags.
    readonly property real targetActiveLeft: draggingActiveTab
        ? dragCurrentX
        : (activeTabIndex >= 0 ? (tabSlotPositions[activeTabIndex] || 0) : retainedActiveLeft)
    readonly property real targetActiveRight: draggingActiveTab
        ? (dragCurrentX + dragWidth)
        : (activeTabIndex >= 0 ? targetActiveLeft + (tabWidths[activeTabIndex] || 0) : retainedActiveRight)

    property real animatedActiveLeft: targetActiveLeft
    property real animatedActiveRight: targetActiveRight

    Behavior on animatedActiveLeft {
        enabled: root.transitionsEnabled && !root.suppressHorizontalAnim && !root.draggingActiveTab && root.activeTabIndex >= 0
        NumberAnimation { id: activeLeftAnim; duration: 160; easing.type: Easing.OutCubic }
    }
    Behavior on animatedActiveRight {
        enabled: root.transitionsEnabled && !root.suppressHorizontalAnim && !root.draggingActiveTab && root.activeTabIndex >= 0
        NumberAnimation { id: activeRightAnim; duration: 160; easing.type: Easing.OutCubic }
    }

    // 0 -> 1 rises the active arch when opening the first tab; 1 -> 0 retracts it when closing the last.
    property real activeContourPresence: activeTabIndex >= 0 ? 1 : 0

    Behavior on activeContourPresence {
        enabled: root.transitionsEnabled
        NumberAnimation { id: contourPresenceAnim; duration: 160; easing.type: Easing.OutCubic }
    }

    readonly property real activeTabLeft: animatedActiveLeft - tabViewport.contentX
    readonly property real activeTabRight: animatedActiveRight - tabViewport.contentX
    readonly property real separatorY: barHeight - outlineWidth / 2
    readonly property real curve: Theme.editorTabRadius
    readonly property bool effectiveAtLeftEdge: activeTabIndex >= 0
        ? (activeTabLeft <= outlineWidth) : retainedActiveAtLeftEdge
    readonly property bool effectiveAtRightEdge: activeTabIndex >= 0
        ? (activeTabRight >= width - outlineWidth) : retainedActiveAtRightEdge

    property real animatedLeftCurve: effectiveAtLeftEdge ? 0 : curve
    property real animatedRightCurve: effectiveAtRightEdge ? 0 : curve

    Behavior on animatedLeftCurve {
        enabled: root.transitionsEnabled && !root.suppressHorizontalAnim && root.activeTabIndex >= 0
        NumberAnimation { id: leftCurveAnim; duration: 140; easing.type: Easing.OutCubic }
    }
    Behavior on animatedRightCurve {
        enabled: root.transitionsEnabled && !root.suppressHorizontalAnim && root.activeTabIndex >= 0
        NumberAnimation { id: rightCurveAnim; duration: 140; easing.type: Easing.OutCubic }
    }

    readonly property bool hasVisibleContour: activeContourPresence > 0.005 && rightEdge > leftEdge
    readonly property real contourTopY: -outlineWidth / 2
        + (1 - activeContourPresence) * (separatorY + outlineWidth / 2)
    readonly property real contourAvailHeight: Math.max(0, separatorY - contourTopY)
    readonly property real leftCurve: Math.min(animatedLeftCurve, contourAvailHeight * 0.5,
        Math.max(0, (rightEdge - leftEdge) * 0.5))
    readonly property real leftEdge: effectiveAtLeftEdge
        ? activeTabLeft - outlineWidth : activeTabLeft
    readonly property real rightCurve: Math.min(animatedRightCurve, contourAvailHeight * 0.5,
        Math.max(0, (rightEdge - leftEdge) * 0.5))
    readonly property real rightEdge: effectiveAtRightEdge
        ? activeTabRight + outlineWidth : activeTabRight

    // One contour supplies both fills and the continuous separator stroke.
    readonly property string tabContour: {
        const k = 0.55228475
        const l = leftEdge, r = rightEdge, c = rightCurve, a = leftCurve
        // The workspace draws the shared top edge; keep this stroke above its clip.
        const t = contourTopY, b = separatorY
        return "M " + (l - a) + " " + b
            + " C " + (l - a * (1 - k)) + " " + b + " " + l + " " + (b - a * (1 - k)) + " " + l + " " + (b - a)
            + " V " + (t + a)
            + " C " + l + " " + (t + a * (1 - k)) + " " + (l + a * (1 - k)) + " " + t + " " + (l + a) + " " + t
            + " H " + (r - c)
            + " C " + (r - c * (1 - k)) + " " + t + " " + r + " " + (t + c * (1 - k)) + " " + r + " " + (t + c)
            + " V " + (b - c)
            + " C " + r + " " + (b - c * (1 - k)) + " " + (r + c * (1 - k)) + " " + b + " " + (r + c) + " " + b
    }
    readonly property string activeFillPath: hasVisibleContour
        ? tabContour + " V " + barHeight + " H " + (leftEdge - leftCurve) + " Z" : ""

    implicitHeight: Theme.workspaceHeaderHeight

    Component.onCompleted: {
        syncTabModel()
        updateRetainedActiveBounds()
        previousActiveIndex = activeTabIndex
        Qt.callLater(() => { root.animationsReady = true })
    }

    // Keep internal strip content at full barHeight anchored to bottom so collapsing
    // root.height from implicitHeight -> 0 slides the strip cleanly without squishing.
    Item {
        id: barContent

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.barHeight

        Shape {
            anchors.fill: parent
            clip: true
            // The compound strip path has a tab-shaped cutout. CurveRenderer can
            // triangulate that hole across the empty strip on macOS/Metal.
            preferredRendererType: Shape.GeometryRenderer

            ShapePath {
                strokeWidth: -1
                fillColor: Theme.surfaceColor(Theme.colors.background.editorTabStrip)
                fillRule: ShapePath.OddEvenFill
                PathSvg {
                    path: "M 0 0 H " + root.width + " V " + root.barHeight + " H 0 Z " + root.activeFillPath
                }
            }
            ShapePath {
                strokeWidth: -1
                fillColor: Theme.surfaceColor(Theme.colors.background.surface)
                PathSvg { path: root.activeFillPath }
            }
            ShapePath {
                strokeWidth: root.outlineWidth
                strokeColor: Theme.separatorColor
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathSvg {
                    path: root.hasVisibleContour
                        ? "M 0 " + root.separatorY + " H " + (root.leftEdge - root.leftCurve)
                            + " " + root.tabContour.replace("M", "L") + " H " + root.width
                        : "M 0 " + root.separatorY + " H " + root.width
                }
            }
        }

        Flickable {
            id: tabViewport

            anchors.left: parent.left
            anchors.right: root.tabsOverflow ? overflowButton.left : parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            contentWidth: tabRow.width
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick
            interactive: !root.isDragging
            onMovementStarted: scrollRevealAnim.stop()
            ScrollBar.horizontal: AppScrollBar {
                policy: root.tabsOverflow ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                height: 3
            }

            HoverHandler {
                cursorShape: root.isDragging ? Qt.ClosedHandCursor : Qt.ArrowCursor
            }

            Item {
                id: tabRow

                width: Math.max(root.totalTabsWidth,
                    root.activeContourPresence > 0.005 ? root.retainedActiveRight : 0)
                height: parent.height

                // Target drop-slot placeholder shown behind the dragged tab.
                Rectangle {
                    id: dropSlotIndicator

                    readonly property color accentColor: Theme.colors.accent.primary
                    visible: root.isDragging && root.dragWidth > 0
                    x: root.dragTargetSlotX + 2
                    y: 3
                    width: Math.max(0, root.dragWidth - 4)
                    height: Math.max(0, parent.height - 6)
                    z: 0
                    radius: Theme.editorTabRadius
                    color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b,
                        Theme.darkTheme ? 0.14 : 0.10)
                    border.width: root.outlineWidth
                    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b,
                        Theme.darkTheme ? 0.55 : 0.45)

                    Behavior on x {
                        enabled: root.animateDropSlot
                        NumberAnimation { id: dropSlotXAnim; duration: 150; easing.type: Easing.OutCubic }
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.max(12, parent.width - 16)
                        height: 2
                        radius: 1
                        color: dropSlotIndicator.accentColor
                    }
                }

                Repeater {
                    id: tabRepeater
                    model: tabModel

                    delegate: AppTab {
                        id: editorTab

                        required property string tabKey
                        required property bool closing
                        required property real closingX
                        required property real closingWidth
                        required property bool collapseWidthOnClose
                        required property string cachedTitle
                        required property string cachedIcon
                        required property string cachedFilledIcon
                        required property int cachedDifficultyId
                        required property bool animateEnter

                        readonly property int liveIndex: closing ? -1 : root.viewState.openEditorTabs.indexOf(tabKey)
                        readonly property bool beingDragged: !closing && root.isDragging && root.draggingEditorKey === tabKey
                        readonly property real liveTargetWidth: liveIndex >= 0
                            ? (root.tabWidths[liveIndex] || root.minimumTabWidth) : closingWidth

                        property real enterProgress: animateEnter ? 0 : 1
                        property real closeProgress: closing ? 1 : 0

                        Behavior on enterProgress {
                            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                        }
                        Behavior on closeProgress {
                            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                        }

                        Component.onCompleted: {
                            if (animateEnter)
                                enterProgress = 1
                        }

                        onClosingChanged: {
                            if (closing)
                                closeCleanupTimer.restart()
                            else
                                closeCleanupTimer.stop()
                        }

                        Timer {
                            id: closeCleanupTimer
                            interval: 160
                            onTriggered: root.finalizeClosingTab(editorTab.tabKey)
                        }

                        clip: closing || enterProgress < 0.999
                        enabled: !closing
                        z: beingDragged ? 10 : closing ? -1 : active ? 1 : 0
                        x: closing
                            ? closingX
                            : beingDragged
                                ? root.dragCurrentX
                                : (liveIndex >= 0 ? (root.tabSlotPositions[liveIndex] || 0) : 0)
                        y: closing
                            ? closeProgress * (collapseWidthOnClose ? 4 : 6)
                            : (1 - enterProgress) * 5
                        width: closing
                            ? (collapseWidthOnClose ? Math.max(0, closingWidth * (1 - closeProgress)) : closingWidth)
                            : beingDragged
                                ? root.dragWidth
                                : (enterProgress < 0.999 && root.tabCount > 1
                                    ? liveTargetWidth * (0.25 + 0.75 * enterProgress)
                                    : liveTargetWidth)
                        height: parent.height
                        preferredTabWidth: closing ? closingWidth : root.preferredWidthForKey(tabKey)
                        showSeparator: root.shouldShowSeparator(liveIndex, tabKey, closing)
                        text: closing
                            ? cachedTitle
                            : ((root.documentSession.dirtyEditorKeys.indexOf(tabKey) >= 0 ? "*" : "")
                               + root.titleForKey(tabKey))
                        secondaryText: ""
                        iconSource: closing ? cachedIcon : root.iconForKey(tabKey)
                        difficultyId: closing ? cachedDifficultyId : root.difficultyIdForKey(tabKey)
                        filledIconSource: closing ? cachedFilledIcon : root.iconForKey(tabKey, true)
                        tooltip: closing ? "" : root.tooltipForKey(tabKey)
                        active: !closing && root.viewState.activeEditorKey === tabKey
                        dragging: beingDragged
                        closable: true
                        opacity: closing
                            ? Math.max(0, 1 - closeProgress)
                            : enterProgress * (beingDragged ? (active ? 0.96 : 0.94) : 1)

                        Behavior on x {
                            enabled: root.transitionsEnabled && !editorTab.beingDragged && !editorTab.closing
                            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                        }
                        Behavior on width {
                            enabled: root.transitionsEnabled
                                && !editorTab.beingDragged
                                && !editorTab.closing
                                && editorTab.enterProgress >= 0.999
                            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                        }

                        onClicked: {
                            if (!closing && !tabDrag.active && root.suppressClickKey !== tabKey)
                                root.activateTab(tabKey)
                        }
                        onCloseRequested: {
                            if (!closing)
                                root.requestCloseTab(tabKey)
                        }

                        DragHandler {
                            id: tabDrag

                            target: null
                            enabled: !editorTab.closing && root.tabCount > 1 && !editorTab.closeButtonPressed
                            acceptedButtons: Qt.LeftButton
                            yAxis.enabled: false

                            onActiveChanged: {
                                if (active) {
                                    scrollRevealAnim.stop()

                                    const pressInRow = tabRow.mapFromItem(
                                        null, centroid.scenePressPosition.x, centroid.scenePressPosition.y)
                                    root.animateDropSlot = false
                                    dropSlotXAnim.stop()
                                    root.dragStartIndex = editorTab.liveIndex
                                    root.dragTargetIndex = editorTab.liveIndex
                                    root.dragWidth = editorTab.width
                                    root.dragPointerOffsetX = pressInRow.x - editorTab.x
                                    root.draggingEditorKey = editorTab.tabKey
                                    root.updateDragPosition(centroid.scenePosition.x, centroid.scenePosition.y)
                                    root.animateDropSlot = true
                                    return
                                }
                                if (root.draggingEditorKey !== editorTab.tabKey)
                                    return

                                const draggedKey = editorTab.tabKey
                                const targetIndex = root.dragTargetIndex

                                root.suppressClickKey = draggedKey
                                Qt.callLater(() => {
                                    if (root.suppressClickKey === draggedKey)
                                        root.suppressClickKey = ""
                                })

                                root.cancelDrag()
                                root.viewState.moveEditorTab(draggedKey, targetIndex)
                            }

                            onCentroidChanged: {
                                if (active && root.draggingEditorKey === editorTab.tabKey)
                                    root.updateDragPosition(centroid.scenePosition.x, centroid.scenePosition.y)
                            }
                        }
                    }
                }
            }
        }

        IconButton {
            id: overflowButton

            anchors.right: parent.right
            anchors.top: parent.top
            width: 30
            height: parent.height
            visible: root.tabsOverflow
            iconSource: Qt.resolvedUrl("icons/more.svg")
            tooltip: qsTrId("qml.show_all_open_editors")
            onClicked: overflowMenu.open()

            background: Rectangle {
                radius: Theme.smallControlRadius
                color: overflowButton.down ? Theme.colors.buttonState.pressed
                    : overflowButton.hovered ? Theme.colors.buttonState.hover : "transparent"
                border.width: overflowButton.visualFocus ? Theme.controlBorderWidth : 0
                border.color: Theme.colors.accent.focus
            }

            AppMenu {
                id: overflowMenu
                x: parent.width - width
                y: parent.height

                Repeater {
                    model: root.viewState.openEditorTabs

                    delegate: AppMenuItem {
                        required property string modelData

                        text: (root.documentSession.dirtyEditorKeys.indexOf(modelData) >= 0 ? "*" : "")
                              + root.titleForKey(modelData)
                        difficultyId: root.difficultyIdForKey(modelData)
                        checkable: true
                        checked: root.viewState.activeEditorKey === modelData
                        onTriggered: root.activateTab(modelData)
                    }
                }
            }
        }
    }

    Connections {
        target: root.documentSession
        function onDifficultyCloseAccepted(difficultyId) {
            root.viewState.closeEditor(root.viewState.difficultyEditorKey(difficultyId))
        }
    }

    Connections {
        target: root.viewState

        function onActiveEditorKeyChanged() {
            root.updateRetainedActiveBounds()
            if (root.previousActiveIndex < 0 && root.activeTabIndex >= 0)
                root.snapHorizontalContour()
            root.previousActiveIndex = root.activeTabIndex
            revealTimer.restart()
        }

        function onOpenEditorTabsChanged() {
            if (root.isDragging && root.viewState.openEditorTabs.indexOf(root.draggingEditorKey) < 0)
                root.cancelDrag()
            root.syncTabModel()
            root.updateRetainedActiveBounds()
            revealTimer.restart()
        }
    }
}
