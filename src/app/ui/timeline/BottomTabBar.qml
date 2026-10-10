pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    required property var documentSession
    required property var analysisSession
    required property var timelineSession

    readonly property int edgePadding: 4
    readonly property int rowVerticalMargin:
        edgePadding - (Theme.controlMinHeight - Theme.controlHighlightHeight) / 2
    readonly property int rowHorizontalMargin:
        edgePadding - Theme.chromeInsetX
    implicitHeight: Theme.controlMinHeight + 2 * rowVerticalMargin
    readonly property real minimumWidth: tabLayout.implicitWidth
        + tabLayout.anchors.leftMargin + tabLayout.anchors.rightMargin

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

    component BottomTab: AppTabButton {
        id: tab

        property int count: -1
        property color countColor: Theme.colors.accent.badge
        readonly property int badgeFontSize: Theme.uiFontSize - 2
        readonly property int badgeHeight: badgeFontSize + 5

        Layout.alignment: Qt.AlignVCenter
        Accessible.description: count > 0 ? String(count) : ""

        accessory: count > 0 ? badgeAccessory : null

        Component {
            id: badgeAccessory
            Rectangle {
                implicitWidth: tab.count > 0
                    ? Math.max(implicitHeight, Math.ceil(countLabel.implicitWidth) + 8) : 0
                implicitHeight: tab.count > 0 ? tab.badgeHeight : 0
                radius: height / 2
                color: tab.countColor
                visible: tab.count > 0

                Text {
                    id: countLabel
                    anchors.fill: parent
                    text: String(tab.count)
                    font.family: Theme.uiFont
                    font.pixelSize: tab.badgeFontSize
                    font.preferTypoLineMetrics: true
                    color: Theme.colors.text.onAccent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    component AnalysisTab: BottomTab {
        required property var issueRows
        readonly property var issueCounts: root.countIssues(issueRows)
        count: issueCounts.errors > 0 ? issueCounts.errors : issueCounts.warnings
        countColor: issueCounts.errors > 0
            ? Theme.colors.danger.primary : Theme.colors.accent.badge
    }

    RowLayout {
        id: tabLayout
        anchors.fill: parent
        anchors.topMargin: root.rowVerticalMargin
        anchors.bottomMargin: root.rowVerticalMargin
        anchors.leftMargin: root.rowHorizontalMargin
        anchors.rightMargin: root.rowHorizontalMargin
        spacing: 4

        BottomTab {
            visible: root.timelineSession.timelineTabVisible
            text: root.timelineSession.timelineTabLabel
            checked: root.timelineSession.currentTabId === "timeline"
            onClicked: root.timelineSession.setCurrentTabId("timeline")
        }
        AnalysisTab {
            visible: root.timelineSession.validationTabVisible
            text: root.timelineSession.validationTabLabel
            issueRows: root.analysisSession.validationRows
            checked: root.timelineSession.currentTabId === "validation"
            onClicked: root.timelineSession.setCurrentTabId("validation")
        }
        AnalysisTab {
            visible: root.timelineSession.muriTabVisible
            text: root.timelineSession.muriTabLabel
            issueRows: root.analysisSession.muriRows
            checked: root.timelineSession.currentTabId === "muri"
            onClicked: root.timelineSession.setCurrentTabId("muri")
        }

        Item { Layout.fillWidth: true }

        IconButton {
            id: settingsButton
            stateColors: Theme.chromeStateColors

            Layout.alignment: Qt.AlignVCenter
            visible: root.timelineSession.currentTabId === "timeline"
            iconSource: Qt.resolvedUrl("icons/sliders-horizontal.svg")
            filledIconSource: Qt.resolvedUrl("icons/sliders-horizontal-fill.svg")
            tooltip: qsTrId("qml.timeline_settings")
            active: settingsMenu.active
            Accessible.description: qsTrId("qml.open_timeline_settings")
            onClicked: {
                if (settingsMenu.active) {
                    settingsMenu.close()
                    return
                }
                settingsMenu.openAt(settingsButton)
            }
        }

    }

    TimelineSettingsMenu {
        id: settingsMenu
        stateBridge: root.timelineSession.stateBridge
        timelineSession: root.timelineSession
    }

}
