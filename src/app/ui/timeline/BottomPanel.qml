pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI
import MiaCode.Timeline

Item {
    id: root

    required property var documentSession
    required property var analysisSession
    required property var preferences
    required property var timelineSession
    required property var previewSession
    property bool guideSettingsActive: false
    property real expandedHeight: 0
    property var tabPresentation: ({
        currentTabId: "",
        timelineTabVisible: false,
        validationTabVisible: false,
        muriTabVisible: false
    })

    function refreshTabPresentation() {
        // Keep the outgoing tabs and page intact until the panel is clipped away.
        if (!root.timelineSession.panelVisible)
            return
        root.tabPresentation = {
            currentTabId: root.timelineSession.currentTabId,
            timelineTabVisible: root.timelineSession.timelineTabVisible,
            validationTabVisible: root.timelineSession.validationTabVisible,
            muriTabVisible: root.timelineSession.muriTabVisible
        }
    }

    Component.onCompleted: refreshTabPresentation()
    Connections {
        target: root.timelineSession
        function onTabsChanged() { root.refreshTabPresentation() }
    }

    readonly property bool guideSettingsAvailable: root.guideSettingsActive
        && root.visible && timelineItem.visible
    readonly property Item timelineGuideItem: timelineItem
    readonly property Item timelineSettingsGuideItem: settingsMenu.contentItem
    signal analysisRowActivated(int difficultyId, var revision, int line, int column, int endColumn, double second)
    signal closeRequested()

    onGuideSettingsAvailableChanged: {
        // Let the menu's focus and close-policy bindings settle before opening.
        if (root.guideSettingsAvailable)
            Qt.callLater(root.openGuideSettingsMenu)
        else
            settingsMenu.close()
    }

    function openGuideSettingsMenu() {
        if (root.guideSettingsAvailable)
            settingsMenu.openAt(brightnessButton)
    }

    function formatSecond(second) {
        if (typeof second !== "number" || !isFinite(second) || second < 0)
            return ""
        const totalCentiseconds = Math.round(second * 100)
        const minutes = Math.floor(totalCentiseconds / 6000)
        const seconds = Math.floor((totalCentiseconds % 6000) / 100)
        const centiseconds = totalCentiseconds % 100
        const mm = String(minutes).padStart(2, "0")
        const ss = String(seconds).padStart(2, "0")
        const cc = String(centiseconds).padStart(2, "0")
        return "%1:%2.%3".arg(mm).arg(ss).arg(cc)
    }

    function toggleActiveSeverityFilter(severity) {
        if (root.tabPresentation.currentTabId === "validation") {
            if (severity === "error")
                root.preferences.validationShowErrors = !root.preferences.validationShowErrors
            else if (severity === "warning")
                root.preferences.validationShowWarnings = !root.preferences.validationShowWarnings
        } else if (root.tabPresentation.currentTabId === "muri") {
            if (severity === "error")
                root.preferences.muriShowErrors = !root.preferences.muriShowErrors
            else if (severity === "warning")
                root.preferences.muriShowWarnings = !root.preferences.muriShowWarnings
        }
    }

    clip: true
    readonly property real minimumHeight: tabs.implicitHeight + timelineItem.minimumViewportHeight
    readonly property real minimumWidth: tabs.minimumWidth
    readonly property int muriWarningCount: root.analysisSession.muriWarningCount
    readonly property int muriIssueCount: root.analysisSession.muriIssueCount
    readonly property var validationCounts: tabs.countIssues(root.analysisSession.validationRows)
    readonly property var filteredValidationRows: {
        const showErrors = root.preferences.validationShowErrors
        const showWarnings = root.preferences.validationShowWarnings
        const rows = root.analysisSession.validationRows
        if (showErrors && showWarnings)
            return rows
        if (!showErrors && !showWarnings)
            return []
        return rows.filter(row => row.severity === "error" ? showErrors : showWarnings)
    }

    Binding {
        target: root.analysisSession
        property: "muriShowErrors"
        value: root.preferences.muriShowErrors
    }
    Binding {
        target: root.analysisSession
        property: "muriShowWarnings"
        value: root.preferences.muriShowWarnings
    }

    Timer {
        id: timelineThemeRefresh
        interval: 0
        onTriggered: timelineItem.refreshTheme()
    }

    component AnalysisIssueRow: AbstractButton {
        id: issueRow

        required property var modelData
        required property string titleText
        required property string bodyText
        property string timeText: ""
        property string locationText: ""

        readonly property bool isError: modelData.severity === "error"
        readonly property color severityColor: isError
            ? Theme.colors.syntax.error
            : Theme.colors.syntax.warning

        width: ListView.view ? ListView.view.width : 0
        implicitHeight: Math.max(44, issueRowLayout.implicitHeight + topPadding + bottomPadding)
        leftPadding: 14
        rightPadding: 14
        topPadding: 7
        bottomPadding: 7
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: issueRow.titleText + " " + issueRow.bodyText
        onClicked: root.analysisSession.activateRow(modelData)

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        background: Item {
            HoverChrome {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                fillBounds: true
                cornerRadius: Theme.controlRadius
                stateColors: Theme.colors.listState
                hovered: issueRow.hovered
                pressed: issueRow.down
                focused: issueRow.visualFocus
            }
        }

        contentItem: RowLayout {
            id: issueRowLayout
            spacing: 10

            ControlsImpl.IconImage {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 1
                width: 16
                height: 16
                source: issueRow.isError
                    ? Qt.resolvedUrl("icons/error-circle-fill.svg")
                    : Qt.resolvedUrl("icons/warning-fill.svg")
                sourceSize: Theme.iconSourceSize(16, 16)
                color: issueRow.severityColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: issueRow.titleText
                        elide: Text.ElideRight
                        color: issueRow.severityColor
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        font.weight: Font.DemiBold
                    }

                    Row {
                        visible: issueRow.timeText.length > 0 || issueRow.locationText.length > 0
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 12

                        Label {
                            visible: issueRow.timeText.length > 0
                            text: issueRow.timeText
                            color: (issueRow.hovered || issueRow.visualFocus)
                                ? Theme.colors.text.primary
                                : Theme.colors.text.secondary
                            font.family: Theme.codeFont.family
                            font.pixelSize: Theme.secondaryFontSize
                        }

                        Label {
                            visible: issueRow.locationText.length > 0
                            text: issueRow.locationText
                            color: (issueRow.hovered || issueRow.visualFocus)
                                ? Theme.colors.text.primary
                                : Theme.colors.text.secondary
                            font.family: Theme.codeFont.family
                            font.pixelSize: Theme.secondaryFontSize
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: issueRow.bodyText
                    color: (issueRow.hovered || issueRow.visualFocus)
                        ? Theme.colors.text.active
                        : Theme.colors.text.primary
                    wrapMode: Text.Wrap
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                }
            }
        }
    }

    component AnalysisEmptyState: Item {
        id: emptyState

        property bool pending: false
        property string text: ""

        anchors.fill: parent

        Column {
            anchors.centerIn: parent
            spacing: 8

            ControlsImpl.IconImage {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 22
                height: 22
                source: emptyState.pending
                    ? Qt.resolvedUrl("icons/more-fill.svg")
                    : Qt.resolvedUrl("icons/checkmark-circle-fill.svg")
                sourceSize: Theme.iconSourceSize(22, 22)
                color: Theme.colors.text.secondary
                opacity: 0.85
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: emptyState.text
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.secondaryFontSize
            }
        }
    }

    // Theme owns timeline surfaces; the palette's timeline group only supplies
    // content colors. The bridge exists before TimelineQuickItem so both sets
    // reach the C++ scene snapshot before its first frame.
    TimelineThemeBridge {
        id: timelineTheme
        windowColor: Theme.surfaceColor(Theme.colors.background.surface)
        headerColor: Theme.surfaceColor(Theme.colors.background.surface)
        sidebarColor: Theme.surfaceColor(Theme.colors.background.panel)
        baseColor: Theme.surfaceColor(Theme.colors.background.surface)
        borderColor: Theme.colors.timeline.border
        axisColor: Theme.colors.timeline.axis
        gridMajorColor: Theme.colors.timeline.gridMajor
        gridSubdivisionColor: Theme.colors.timeline.gridSubdivision
        gridMinorColor: Theme.colors.timeline.gridMinor
        laneEvenColor: Theme.contentOverlayColor(Theme.colors.timeline.laneEven)
        laneOddColor: Theme.contentOverlayColor(Theme.colors.timeline.laneOdd)
        labelColor: Theme.colors.timeline.label
        textSecondaryColor: Theme.colors.timeline.textSecondary
        waveStrokeColor: Theme.colors.timeline.waveStroke
        onWindowColorChanged: timelineThemeRefresh.restart()
        onHeaderColorChanged: timelineThemeRefresh.restart()
        onSidebarColorChanged: timelineThemeRefresh.restart()
        onBaseColorChanged: timelineThemeRefresh.restart()
        onBorderColorChanged: timelineThemeRefresh.restart()
        onAxisColorChanged: timelineThemeRefresh.restart()
        onGridMajorColorChanged: timelineThemeRefresh.restart()
        onGridSubdivisionColorChanged: timelineThemeRefresh.restart()
        onGridMinorColorChanged: timelineThemeRefresh.restart()
        onLaneEvenColorChanged: timelineThemeRefresh.restart()
        onLaneOddColorChanged: timelineThemeRefresh.restart()
        onLabelColorChanged: timelineThemeRefresh.restart()
        onTextSecondaryColorChanged: timelineThemeRefresh.restart()
        onWaveStrokeColorChanged: timelineThemeRefresh.restart()
    }

    // Keep inner content at its expanded height while the panel slides open or
    // closed so the timeline and issue lists slide smoothly instead of squishing.
    Item {
        id: panelContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.expandedHeight > 0 ? Math.max(parent.height, root.expandedHeight) : parent.height

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabs.bottom
            anchors.bottom: parent.bottom
            visible: !timelineItem.visible
            color: Theme.surfaceColor(Theme.colors.background.surface)
        }

        BottomTabBar {
            id: tabs
            presentation: root.tabPresentation
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            documentSession: root.documentSession
            analysisSession: root.analysisSession
            timelineSession: root.timelineSession
            validationShowErrors: root.preferences.validationShowErrors
            validationShowWarnings: root.preferences.validationShowWarnings
            muriShowErrors: root.preferences.muriShowErrors
            muriShowWarnings: root.preferences.muriShowWarnings
            activeErrorCount: root.tabPresentation.currentTabId === "muri"
                ? root.muriIssueCount
                : root.validationCounts.errors
            activeWarningCount: root.tabPresentation.currentTabId === "muri"
                ? root.muriWarningCount
                : root.validationCounts.warnings
            activeAnalysisPending: root.tabPresentation.currentTabId === "muri"
                ? root.analysisSession.pending
                : root.analysisSession.validationPending
            onSeverityFilterToggled: severity => root.toggleActiveSeverityFilter(severity)
            onCloseRequested: root.closeRequested()
        }

        TimelineQuickItem {
            id: timelineItem

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabs.bottom
            anchors.bottom: parent.bottom
            visible: root.tabPresentation.currentTabId === "timeline"
            enabled: visible
            stateBridge: root.timelineSession.stateBridge
            headerLeftLimit: 0
            headerRightLimit: 0
            headerMarkerLeftLimit: 0
            headerMarkerRightLimit: 0
            onHeaderNavigateRequested: second => root.timelineSession.headerNavigate(second)
            onTimelineWheelNavigateRequested: second => root.timelineSession.wheelNavigate(second)
            onCenterNavigateRequested: second => root.timelineSession.centerNavigate(second)
            onTimelineDragStarted: root.timelineSession.dragStarted()
            onTimelineDragFinished: second => root.timelineSession.dragFinished(second)
            onTimelineUserInteractionStarted: root.timelineSession.userInteractionStarted()
            onTimelineSurfaceReady: root.timelineSession.surfaceReady()
            onFollowPreviewToggled: enabled => root.timelineSession.followPreviewToggled(enabled)
        }

        Item {
            id: timelineControlsDock

            anchors.right: timelineItem.right
            anchors.bottom: timelineItem.bottom
            anchors.rightMargin: 10
            anchors.bottomMargin: 8
            z: 5
            visible: timelineItem.visible
            implicitWidth: dockRow.implicitWidth + 8
            implicitHeight: Math.max(zoomButton.height, brightnessButton.implicitHeight) + 6

            readonly property bool engaged: dockHover.hovered
                || dockBlocker.containsMouse
                || zoomButton.hovered
                || zoomButton.activeFocus
                || zoomButton.menuActive
                || brightnessButton.hovered
                || brightnessButton.activeFocus
                || settingsMenu.active
                || root.guideSettingsActive

            opacity: engaged ? 0.96 : 0.48

            Behavior on opacity {
                enabled: Theme.animationsEnabled
                NumberAnimation { duration: Theme.animDuration(150); easing.type: Easing.OutCubic }
            }

            HoverHandler {
                id: dockHover
            }

            MouseArea {
                id: dockBlocker
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.AllButtons
                preventStealing: true
                onPressed: mouse => { mouse.accepted = true }
                onWheel: wheel => { wheel.accepted = true }
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.controlRadius + 2
                readonly property color baseTone: Theme.colors.background.elevated
                color: Qt.rgba(
                    baseTone.r,
                    baseTone.g,
                    baseTone.b,
                    Theme.darkTheme
                        ? (timelineControlsDock.engaged ? 0.84 : 0.62)
                        : (timelineControlsDock.engaged ? 0.90 : 0.72))
                border.width: 1 / Screen.devicePixelRatio
                border.color: Theme.floatingBorderColor
                border.pixelAligned: false

                Behavior on color {
                    enabled: Theme.animationsEnabled
                    ColorAnimation { duration: Theme.animDuration(150); easing.type: Easing.OutCubic }
                }
            }

            Row {
                id: dockRow
                anchors.centerIn: parent
                spacing: 4

                TimelineZoomControl {
                    id: zoomButton
                    anchors.verticalCenter: parent.verticalCenter
                    borderless: true
                    height: Theme.compactControlHeight
                    stateBridge: root.timelineSession.stateBridge
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1 / Screen.devicePixelRatio
                    height: 14
                    color: Theme.floatingBorderColor
                }

                IconButton {
                    id: brightnessButton
                    anchors.verticalCenter: parent.verticalCenter
                    compact: true
                    iconSource: Qt.resolvedUrl("icons/sliders-horizontal-fill.svg")
                    filledIconSource: Qt.resolvedUrl("icons/sliders-horizontal-fill.svg")
                    iconWidth: 15
                    iconHeight: 15
                    tooltip: qsTrId("qml.timeline_settings")
                    active: settingsMenu.active
                    Accessible.description: qsTrId("qml.open_timeline_settings")
                    onClicked: {
                        if (settingsMenu.active) {
                            settingsMenu.close()
                            return
                        }
                        settingsMenu.openAt(brightnessButton)
                    }
                }
            }
        }

        Tooltip {
            id: timelineMarkerTooltip
            parent: Overlay.overlay
            visible: timelineItem.hoverTooltipText.length > 0
            text: timelineItem.hoverTooltipText
            x: {
                const position = timelineItem.mapToItem(parent, timelineItem.hoverTooltipPosition)
                return position.x + 12
            }
            y: {
                const position = timelineItem.mapToItem(parent, timelineItem.hoverTooltipPosition)
                return position.y + 16
            }
        }

        TimelineSettingsMenu {
            id: settingsMenu
            stateBridge: root.timelineSession.stateBridge
            timelineSession: root.timelineSession
            // The modal guide owns input and Escape while demonstrating this menu.
            focus: !root.guideSettingsActive
            closePolicy: root.guideSettingsActive ? Popup.NoAutoClose
                : Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
            z: root.guideSettingsActive ? 99 : 0
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabs.bottom
            anchors.bottom: parent.bottom
            visible: root.tabPresentation.currentTabId === "validation"

            ListView {
                id: validationList
                anchors.fill: parent
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 2
                clip: true
                model: root.filteredValidationRows

                delegate: AnalysisIssueRow {
                    titleText: modelData.code === "missing_difficulty_level"
                        ? qsTrId("validation.difficulty_header")
                        : (modelData.severity === "error"
                            ? qsTrId("validation.severity.error")
                            : qsTrId("validation.severity.warning"))
                    bodyText: modelData.detail
                    locationText: modelData.code === "missing_difficulty_level"
                        ? ""
                        : "L%1:C%2".arg(modelData.line).arg(modelData.column)
                }

                ScrollBar.vertical: AppScrollBar {}
            }

            AnalysisEmptyState {
                visible: root.analysisSession.validationPending
                    || root.analysisSession.validationRows.length === 0
                pending: root.analysisSession.validationPending
                text: pending
                    ? qsTrId("qml.analyzing")
                    : qsTrId("validation.no_syntax_errors_detected")
            }
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabs.bottom
            anchors.bottom: parent.bottom
            visible: root.tabPresentation.currentTabId === "muri"

            ListView {
                id: muriList
                anchors.fill: parent
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 2
                clip: true
                model: root.analysisSession.muriRows

                delegate: AnalysisIssueRow {
                    titleText: modelData.title
                    bodyText: modelData.detail
                    timeText: root.formatSecond(modelData.second)
                    locationText: "L%1:C%2".arg(modelData.line).arg(modelData.column)
                }

                ScrollBar.vertical: AppScrollBar {}
            }

            AnalysisEmptyState {
                visible: root.analysisSession.pending
                    || root.analysisSession.muriRowCount === 0
                pending: root.analysisSession.pending
                text: pending
                    ? qsTrId("qml.analyzing")
                    : qsTrId("validation.no_muri_issues_detected")
            }
        }

        // The panel bounds let the HUD extend above a compact note viewport.
        // Keep it above every bottom tab so tab switches preserve the notification.
        PreviewRateToast {
            anchors.fill: parent
            noteViewportRect: Qt.rect(
                timelineItem.x + timelineItem.noteViewportRect.x,
                timelineItem.y + timelineItem.noteViewportRect.y,
                timelineItem.noteViewportRect.width,
                timelineItem.noteViewportRect.height)
            previewSession: root.previewSession
        }

        // Keep the inner content wrapper covering every visual child except the
        // non-visual helpers below.
    }

    Connections {
        target: root.analysisSession
        function onRowActivated(difficultyId, revision, line, column, endColumn, second) {
            root.analysisRowActivated(difficultyId, revision, line, column, endColumn, second)
        }
    }
}
