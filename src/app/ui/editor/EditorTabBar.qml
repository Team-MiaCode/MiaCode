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
    readonly property real availableTabWidth: Math.max(0, width - (tabsOverflow ? overflowButton.width : 0))
    readonly property real activePreferredWidth: activeTabIndex >= 0
        ? preferredTabWidths[activeTabIndex] : 0
    readonly property bool tabsOverflow: activeTabIndex >= 0
        ? activePreferredWidth + (tabCount - 1) * minimumTabWidth > width
        : tabCount * minimumTabWidth > width
    readonly property var preferredTabWidths: viewState.openEditorTabs.map(key => {
        const title = (documentSession.dirtyEditorKeys.indexOf(key) >= 0 ? "*" : "") + titleForKey(key)
        const iconWidth = difficultyIdForKey(key) > 0 ? Theme.difficultySwatchSize
            : key === viewState.metadataEditorKey || key === viewState.latencyEditorKey ? 15 : 0
        // Match AppTab's content margins, close-button slot and visible row gaps.
        return Math.max(minimumTabWidth,
            Math.ceil(tabFontMetrics.advanceWidth(title) + 14 + 9 + 24 + 6 + (iconWidth > 0 ? iconWidth + 6 : 0)))
    })
    readonly property real preferredTabsWidth: preferredTabWidths.reduce((sum, value) => sum + value, 0)
    readonly property var tabWidths: {
        const otherCount = tabCount - (activeTabIndex >= 0 ? 1 : 0)
        const otherPreferredWidth = preferredTabsWidth - activePreferredWidth
        const minimumOtherWidth = otherCount * minimumTabWidth
        const remainingWidth = availableTabWidth - activePreferredWidth
        const scale = otherPreferredWidth > minimumOtherWidth
            ? Math.max(0, Math.min(1, (remainingWidth - minimumOtherWidth)
                / (otherPreferredWidth - minimumOtherWidth))) : 1
        return preferredTabWidths.map((value, index) => index === activeTabIndex
            ? value : minimumTabWidth + (value - minimumTabWidth) * scale)
    }

    FontMetrics {
        id: tabFontMetrics
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
    }

    onTabWidthsChanged: revealTimer.restart()

    Timer {
        id: revealTimer
        interval: 0
        onTriggered: root.revealActiveTab()
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
            result += qsTrId("qml.chart_designer_1").arg(difficulty.designer)
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
        if (key === viewState.metadataEditorKey && root.pages.activePageId === "latency"
                && !root.pages.activateMetadataPage())
            return
        viewState.activateEditor(key)
    }

    function editorKeyAt(rowX) {
        for (let index = 0; index < tabRepeater.count; ++index) {
            const key = root.viewState.openEditorTabs[index]
            const item = tabRepeater.itemAt(index)
            if (!item)
                continue
            if (rowX >= item.x && rowX < item.x + item.width)
                return key
        }
        return ""
    }

    function revealActiveTab() {
        const index = viewState.openEditorTabs.indexOf(viewState.activeEditorKey)
        const item = index >= 0 ? tabRepeater.itemAt(index) : null
        if (!item)
            return
        if (item.x < tabViewport.contentX)
            tabViewport.contentX = item.x
        else if (item.x + item.width > tabViewport.contentX + tabViewport.width)
            tabViewport.contentX = item.x + item.width - tabViewport.width
    }

    readonly property real outlineWidth: 1 / Screen.devicePixelRatio
    readonly property int activeTabIndex: viewState.openEditorTabs.indexOf(viewState.activeEditorKey)
    readonly property real activeTabLeft: tabWidths.slice(0, Math.max(0, activeTabIndex))
        .reduce((sum, value) => sum + value, 0) - tabViewport.contentX
    readonly property real activeTabRight: activeTabLeft + (tabWidths[activeTabIndex] || 0)
    readonly property real separatorY: height - outlineWidth / 2
    readonly property real curve: Theme.editorTabRadius
    readonly property real leftCurve: activeTabIndex === 0 ? 0 : curve
    readonly property real leftEdge: activeTabIndex === 0
        ? activeTabLeft - outlineWidth : activeTabLeft
    readonly property bool activeTabAtRightEdge: activeTabRight >= width - outlineWidth
    readonly property real rightCurve: activeTabAtRightEdge ? 0 : curve
    readonly property real rightEdge: activeTabAtRightEdge
        ? activeTabRight + outlineWidth : activeTabRight

    // One contour supplies both fills and the continuous separator stroke.
    readonly property string tabContour: {
        const k = 0.55228475
        const l = leftEdge, r = rightEdge, c = rightCurve, a = leftCurve
        // The workspace draws the shared top edge; keep this stroke above its clip.
        const t = -outlineWidth / 2, b = separatorY
        return "M " + (l - a) + " " + b
            + " C " + (l - a * (1 - k)) + " " + b + " " + l + " " + (b - a * (1 - k)) + " " + l + " " + (b - a)
            + " V " + (t + a)
            + " C " + l + " " + (t + a * (1 - k)) + " " + (l + a * (1 - k)) + " " + t + " " + (l + a) + " " + t
            + " H " + (r - c)
            + " C " + (r - c * (1 - k)) + " " + t + " " + r + " " + (t + c * (1 - k)) + " " + r + " " + (t + c)
            + " V " + (b - c)
            + " C " + r + " " + (b - c * (1 - k)) + " " + (r + c * (1 - k)) + " " + b + " " + (r + c) + " " + b
    }
    readonly property string activeFillPath: activeTabIndex >= 0
        ? tabContour + " V " + height + " H " + (leftEdge - leftCurve) + " Z" : ""

    implicitHeight: Theme.workspaceHeaderHeight

    Shape {
        anchors.fill: parent
        clip: true
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: -1
            fillColor: Theme.surfaceColor(Theme.colors.background.editorTabStrip)
            fillRule: ShapePath.OddEvenFill
            PathSvg {
                path: "M 0 0 H " + root.width + " V " + root.height + " H 0 Z " + root.activeFillPath
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
                path: root.activeTabIndex >= 0
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
        interactive: root.draggingEditorKey.length === 0
        ScrollBar.horizontal: AppScrollBar {
            policy: root.tabsOverflow ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            height: 3
        }

        Row {
            id: tabRow

            width: childrenRect.width
            height: parent.height
            spacing: 0

            Repeater {
                id: tabRepeater
                model: root.viewState.openEditorTabs

                delegate: AppTab {
                    required property string modelData
                    required property int index

                    property bool suppressClickAfterDrag: false

                    width: root.tabWidths[index]
                    height: parent.height
                    preferredTabWidth: root.preferredTabWidths[index]
                    showSeparator: index < root.tabCount - 1
                        && root.viewState.openEditorTabs[index + 1] !== root.viewState.activeEditorKey
                    text: (root.documentSession.dirtyEditorKeys.indexOf(modelData) >= 0 ? "*" : "")
                          + root.titleForKey(modelData)
                    secondaryText: ""
                    iconSource: modelData === root.viewState.metadataEditorKey
                        ? Qt.resolvedUrl("icons/metadata.svg")
                        : modelData === root.viewState.latencyEditorKey
                            ? Qt.resolvedUrl("icons/metronome.svg") : ""
                    difficultyId: root.difficultyIdForKey(modelData)
                    filledIconSource: modelData === root.viewState.metadataEditorKey
                        ? Qt.resolvedUrl("icons/metadata-fill.svg")
                        : modelData === root.viewState.latencyEditorKey
                            ? Qt.resolvedUrl("icons/metronome-fill.svg") : ""
                    tooltip: root.tooltipForKey(modelData)
                    active: root.viewState.activeEditorKey === modelData
                    closable: true
                    opacity: tabDrag.active ? 0.65 : 1
                    onClicked: {
                        if (!tabDrag.active && !suppressClickAfterDrag)
                            root.activateTab(modelData)
                    }
                    onCloseRequested: root.requestCloseTab(modelData)

                    DragHandler {
                        id: tabDrag

                        target: null
                        acceptedButtons: Qt.LeftButton

                        onActiveChanged: {
                            if (active) {
                                root.draggingEditorKey = modelData
                                return
                            }
                            if (root.draggingEditorKey !== modelData)
                                return
                            root.draggingEditorKey = ""
                            const positionInRow = tabRow.mapFromItem(
                                parent, centroid.position.x, centroid.position.y)
                            const targetKey = root.editorKeyAt(positionInRow.x)
                            if (targetKey.length > 0)
                                root.viewState.swapEditorTabs(modelData, targetKey)
                            suppressClickAfterDrag = true
                            Qt.callLater(function() { suppressClickAfterDrag = false })
                        }
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
    property string draggingEditorKey: ""

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

    Connections {
        target: root.viewState

        function onActiveEditorKeyChanged() {
            revealTimer.restart()
        }

        function onOpenEditorTabsChanged() {
            revealTimer.restart()
        }
    }
}
