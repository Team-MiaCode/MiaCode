import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    required property var documentSession
    required property var analysisSession
    required property var timelineSession

    readonly property int edgePadding: 5
    readonly property int rowVerticalMargin:
        edgePadding - Theme.chromeInsetY + Theme.chromeHighlightOutset
    readonly property int rowHorizontalMargin:
        edgePadding - Theme.chromeInsetX + Theme.chromeHighlightOutset
    implicitHeight: Theme.compactControlHeight + 2 * rowVerticalMargin
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

    component BottomTab: AbstractButton {
        id: tab

        property int count: -1
        property color countColor: Theme.colors.accent.badge
        readonly property int badgeFontSize: Theme.uiFontSize - 2
        readonly property int badgeHeight: badgeFontSize + 5

        implicitWidth: contentRow.implicitWidth + leftPadding + rightPadding
        implicitHeight: Math.max(24, title.implicitHeight + 6, badgeHeight + 6)
        leftPadding: Theme.compactTabContentPadding
        rightPadding: leftPadding
        topPadding: 0
        bottomPadding: 0
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        checkable: true
        Layout.alignment: Qt.AlignVCenter
        Accessible.name: text
        Accessible.description: count > 0 ? String(count) : ""

        contentItem: Row {
            id: contentRow
            spacing: 6

            Text {
                id: title
                width: implicitWidth
                height: contentRow.height
                text: tab.text
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                font.preferTypoLineMetrics: true
                color: tab.checked || tab.hovered || tab.visualFocus
                    ? Theme.colors.text.active : Theme.colors.text.secondary
                verticalAlignment: Text.AlignVCenter
            }

            Rectangle {
                width: Math.max(height, Math.ceil(countLabel.implicitWidth) + 8)
                height: tab.badgeHeight
                anchors.verticalCenter: parent.verticalCenter
                visible: tab.count > 0
                radius: height / 2
                color: tab.countColor

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

        background: HoverChrome {
            cornerRadius: Theme.compactControlRadius
            stateColors: Theme.colors.popupState
            contentHeight: Math.max(title.implicitHeight, tab.badgeHeight)
            selected: tab.checked
            hovered: tab.hovered
            pressed: tab.down
            focused: tab.visualFocus
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
            implicitWidth: 24
            implicitHeight: 24
            iconWidth: 16
            iconHeight: 16
            cornerRadius: Theme.compactControlRadius
            highlightOutset: Theme.chromeHighlightOutset
            stateColors: Theme.colors.popupState

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
