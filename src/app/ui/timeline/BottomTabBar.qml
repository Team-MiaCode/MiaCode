pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window
import MiaCode.UI

Item {
    id: root

    required property var documentSession
    required property var analysisSession
    required property var timelineSession
    required property var presentation

    property bool validationShowErrors: true
    property bool validationShowWarnings: true
    property bool muriShowErrors: true
    property bool muriShowWarnings: true
    property int activeErrorCount: 0
    property int activeWarningCount: 0
    property bool activeAnalysisPending: false
    readonly property bool activeShowErrors: root.presentation.currentTabId === "muri"
        ? root.muriShowErrors
        : root.validationShowErrors
    readonly property bool activeShowWarnings: root.presentation.currentTabId === "muri"
        ? root.muriShowWarnings
        : root.validationShowWarnings

    signal severityFilterToggled(string severity)
    signal closeRequested()

    readonly property int edgePadding: 6
    readonly property real outlineWidth: 1 / Screen.devicePixelRatio
    readonly property real separatorY: height - outlineWidth / 2
    readonly property real curve: Theme.editorTabRadius
    readonly property bool analysisTabActive: root.presentation.currentTabId === "validation"
        || root.presentation.currentTabId === "muri"
    readonly property Item activeTabItem: {
        if (root.presentation.currentTabId === "timeline" && timelineTab.visible)
            return timelineTab
        if (root.presentation.currentTabId === "validation" && validationTab.visible)
            return validationTab
        if (root.presentation.currentTabId === "muri" && muriTab.visible)
            return muriTab
        return null
    }

    property bool animationsReady: false
    readonly property real targetActiveLeft: activeTabItem ? activeTabItem.x : 0
    readonly property real targetActiveRight: activeTabItem ? (activeTabItem.x + activeTabItem.width) : 0
    property real animatedActiveLeft: targetActiveLeft
    property real animatedActiveRight: targetActiveRight

    Behavior on animatedActiveLeft {
        enabled: root.animationsReady && root.activeTabItem !== null
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }
    Behavior on animatedActiveRight {
        enabled: root.animationsReady && root.activeTabItem !== null
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    readonly property bool effectiveAtLeftEdge: activeTabItem !== null
        && animatedActiveLeft <= outlineWidth
    readonly property bool effectiveAtRightEdge: activeTabItem !== null
        && animatedActiveRight >= width - outlineWidth
    property real animatedLeftCurve: effectiveAtLeftEdge ? 0 : curve
    property real animatedRightCurve: effectiveAtRightEdge ? 0 : curve

    Behavior on animatedLeftCurve {
        enabled: root.animationsReady && root.activeTabItem !== null
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }
    Behavior on animatedRightCurve {
        enabled: root.animationsReady && root.activeTabItem !== null
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    readonly property real leftEdge: effectiveAtLeftEdge
        ? animatedActiveLeft - outlineWidth : animatedActiveLeft
    readonly property real rightEdge: effectiveAtRightEdge
        ? animatedActiveRight + outlineWidth : animatedActiveRight
    readonly property bool hasVisibleContour: activeTabItem !== null && rightEdge > leftEdge
    readonly property real contourTopY: -outlineWidth / 2
    readonly property real contourAvailHeight: Math.max(0, separatorY - contourTopY)
    readonly property real leftCurve: Math.min(animatedLeftCurve, contourAvailHeight * 0.5,
        Math.max(0, (rightEdge - leftEdge) * 0.5))
    readonly property real rightCurve: Math.min(animatedRightCurve, contourAvailHeight * 0.5,
        Math.max(0, (rightEdge - leftEdge) * 0.5))
    readonly property string tabContour: {
        const k = 0.55228475
        const l = leftEdge, r = rightEdge, c = rightCurve, a = leftCurve
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
        ? tabContour + " V " + height + " H " + (leftEdge - leftCurve) + " Z" : ""

    implicitHeight: Theme.workspaceHeaderHeight
    readonly property real minimumWidth: tabRow.implicitWidth + trailingRow.implicitWidth + 16

    Component.onCompleted: Qt.callLater(() => { root.animationsReady = true })

    function countIssues(rows) {
        let errors = 0
        let warnings = 0
        for (const row of rows) {
            if (row.severity === "error")
                ++errors
            else if (row.severity === "warning")
                ++warnings
        }
        return { errors: errors, warnings: warnings }
    }

    component PanelTab: AbstractButton {
        id: tab

        property int errorCount: 0
        property int warningCount: 0
        property bool showSeparator: false

        implicitWidth: Math.max(64, tabContent.implicitWidth + 28)
        implicitHeight: Theme.workspaceHeaderHeight
        height: parent ? parent.height : implicitHeight
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        checkable: true
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        font.weight: tab.checked ? Font.DemiBold : Font.Normal
        font.preferTypoLineMetrics: true
        Accessible.role: Accessible.PageTab
        Accessible.description: (tab.errorCount > 0 || tab.warningCount > 0)
            ? (tab.errorCount > 0 && tab.warningCount > 0
                ? "%1 %2".arg(tab.errorCount).arg(tab.warningCount)
                : String(tab.errorCount > 0 ? tab.errorCount : tab.warningCount))
            : ""

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: 3
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                radius: Theme.editorTabRadius
                visible: !tab.checked
                color: tab.down
                    ? Theme.overlayColor(Theme.colors.state.pressed)
                    : tab.hovered
                        ? Theme.overlayColor(Theme.colors.state.hover)
                        : "transparent"

                Behavior on color {
                    ColorAnimation { duration: 110; easing.type: Easing.OutCubic }
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: root.outlineWidth
                height: Theme.uiFontSize + 3
                opacity: tab.showSeparator && !tab.checked && !tab.hovered ? 1 : 0
                color: Theme.separatorColor

                Behavior on opacity {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Theme.controlBorderWidth * 2
                radius: Theme.smallControlRadius
                visible: tab.visualFocus
                color: "transparent"
                border.width: Theme.controlBorderWidth
                border.color: Theme.colors.accent.focus
            }
        }

        contentItem: Item {
            implicitWidth: tabContent.implicitWidth
            implicitHeight: tabContent.implicitHeight

            Row {
                id: tabContent
                anchors.centerIn: parent
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: tab.text
                    font: tab.font
                    color: (tab.checked || tab.hovered || tab.visualFocus)
                        ? Theme.colors.text.active
                        : Theme.colors.text.secondary

                    Behavior on color {
                        ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
                    }
                }

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: tab.errorCount > 0 || tab.warningCount > 0
                    spacing: 4

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.errorCount > 0
                        implicitHeight: Math.max(16, Theme.captionFontSize + 6)
                        implicitWidth: Math.max(implicitHeight, Math.ceil(errorBadgeLabel.implicitWidth) + 10)
                        radius: height / 2
                        color: Theme.colors.danger.primary

                        Text {
                            id: errorBadgeLabel
                            anchors.centerIn: parent
                            text: String(tab.errorCount)
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.captionFontSize
                            font.weight: Font.DemiBold
                            font.preferTypoLineMetrics: true
                            color: Theme.colors.text.onDanger
                        }
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.warningCount > 0
                        implicitHeight: Math.max(16, Theme.captionFontSize + 6)
                        implicitWidth: Math.max(implicitHeight, Math.ceil(warningBadgeLabel.implicitWidth) + 10)
                        radius: height / 2
                        color: Theme.colors.accent.badge

                        Text {
                            id: warningBadgeLabel
                            anchors.centerIn: parent
                            text: String(tab.warningCount)
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.captionFontSize
                            font.weight: Font.DemiBold
                            font.preferTypoLineMetrics: true
                            color: Theme.colors.text.onAccent
                        }
                    }
                }
            }
        }
    }

    component AnalysisTab: PanelTab {
        required property var issueCounts
        required property bool showErrors
        required property bool showWarnings
        errorCount: showErrors ? issueCounts.errors : 0
        warningCount: showWarnings ? issueCounts.warnings : 0
    }

    component SeverityFilterButton: AbstractButton {
        id: filterButton

        required property url iconSource
        required property color iconColor
        required property int count
        property bool active: false
        property string tooltip: ""

        implicitHeight: Theme.compactControlHeight
        implicitWidth: filterRow.implicitWidth + 14
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: filterButton.tooltip.length > 0
            ? filterButton.tooltip : String(filterButton.count)
        Accessible.description: String(filterButton.count)

        background: Rectangle {
            radius: Theme.compactControlRadius
            color: filterButton.down
                ? Theme.overlayColor(Theme.colors.buttonState.pressed)
                : filterButton.hovered
                    ? Theme.overlayColor(Theme.colors.buttonState.hover)
                    : filterButton.active
                        ? Theme.overlayColor(Theme.colors.buttonState.selected)
                        : "transparent"
            border.width: filterButton.visualFocus
                ? Theme.controlBorderWidth
                : (filterButton.active ? root.outlineWidth : 0)
            border.color: filterButton.visualFocus
                ? Theme.colors.accent.focus
                : Theme.floatingBorderColor
        }

        contentItem: Item {
            implicitWidth: filterRow.implicitWidth
            implicitHeight: filterRow.implicitHeight

            Row {
                id: filterRow
                anchors.centerIn: parent
                spacing: 5
                opacity: filterButton.active ? 1.0 : 0.4

                ControlsImpl.IconImage {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    source: filterButton.iconSource
                    sourceSize: Theme.iconSourceSize(14, 14)
                    color: filterButton.iconColor
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: String(filterButton.count)
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.secondaryFontSize
                    font.weight: filterButton.active ? Font.DemiBold : Font.Normal
                    font.preferTypoLineMetrics: true
                    color: !filterButton.active
                        ? Theme.colors.text.disabled
                        : (filterButton.hovered || filterButton.visualFocus)
                            ? Theme.colors.text.active
                            : Theme.colors.text.primary
                }
            }
        }

        Tooltip {
            visible: filterButton.hovered && filterButton.tooltip.length > 0
            text: filterButton.tooltip
        }
    }

    Shape {
        anchors.fill: parent
        clip: true
        preferredRendererType: Shape.GeometryRenderer

        ShapePath {
            strokeWidth: -1
            fillColor: Theme.surfaceColor(Theme.colors.background.editorTabStrip)
            fillRule: ShapePath.OddEvenFill
            PathSvg {
                path: root.hasVisibleContour
                    ? "M 0 0 H " + root.width + " V " + root.height + " H 0 Z " + root.activeFillPath
                    : "M 0 0 H " + root.width + " V " + root.height + " H 0 Z"
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

    Row {
        id: tabRow
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0

        PanelTab {
            id: timelineTab
            visible: root.presentation.timelineTabVisible
            text: root.timelineSession.timelineTabLabel
            checked: root.presentation.currentTabId === "timeline"
            showSeparator: (validationTab.visible && !validationTab.checked && !validationTab.hovered)
                || (!validationTab.visible && muriTab.visible && !muriTab.checked && !muriTab.hovered)
            onClicked: root.timelineSession.setCurrentTabId("timeline")
        }

        AnalysisTab {
            id: validationTab
            visible: root.presentation.validationTabVisible
            text: root.timelineSession.validationTabLabel
            issueCounts: root.countIssues(root.analysisSession.validationRows)
            showErrors: root.validationShowErrors
            showWarnings: root.validationShowWarnings
            checked: root.presentation.currentTabId === "validation"
            showSeparator: muriTab.visible && !muriTab.checked && !muriTab.hovered
            onClicked: root.timelineSession.setCurrentTabId("validation")
        }

        AnalysisTab {
            id: muriTab
            visible: root.presentation.muriTabVisible
            text: root.timelineSession.muriTabLabel
            issueCounts: ({
                errors: root.analysisSession.muriIssueCount,
                warnings: root.analysisSession.muriWarningCount
            })
            showErrors: root.muriShowErrors
            showWarnings: root.muriShowWarnings
            checked: root.presentation.currentTabId === "muri"
            onClicked: root.timelineSession.setCurrentTabId("muri")
        }
    }

    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: root.edgePadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.analysisTabActive && root.activeAnalysisPending
            text: qsTrId("qml.analyzing")
            font.family: Theme.uiFont
            font.pixelSize: Theme.secondaryFontSize
            font.preferTypoLineMetrics: true
            color: Theme.colors.text.secondary
        }

        SeverityFilterButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.analysisTabActive && !root.activeAnalysisPending
            iconSource: Qt.resolvedUrl("icons/error-circle-fill.svg")
            iconColor: Theme.colors.syntax.error
            count: root.activeErrorCount
            active: root.activeShowErrors
            tooltip: root.presentation.currentTabId === "muri"
                ? qsTrId("validation.muri.alert.muri")
                : qsTrId("validation.severity.error")
            onClicked: root.severityFilterToggled("error")
        }

        SeverityFilterButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.analysisTabActive && !root.activeAnalysisPending
            iconSource: Qt.resolvedUrl("icons/warning-fill.svg")
            iconColor: Theme.colors.syntax.warning
            count: root.activeWarningCount
            active: root.activeShowWarnings
            tooltip: root.presentation.currentTabId === "muri"
                ? qsTrId("validation.muri.alert.warning")
                : qsTrId("validation.severity.warning")
            onClicked: root.severityFilterToggled("warning")
        }

        Item {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.analysisTabActive
            width: 5
            height: 14

            Rectangle {
                anchors.centerIn: parent
                width: root.outlineWidth
                height: parent.height
                color: Theme.separatorColor
            }
        }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            compact: true
            iconSource: Qt.resolvedUrl("icons/close-bold.svg")
            iconWidth: 14
            iconHeight: 14
            tooltip: qsTrId("qml.close_bottom_panel")
            Accessible.name: qsTrId("qml.close_bottom_panel")
            onClicked: root.closeRequested()
        }
    }
}
