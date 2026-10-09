import QtQuick
import QtQuick.Controls
import MiaCode.UI

Rectangle {
    id: root

    required property var viewState
    required property var documentSession
    required property var commands
    property var pages
    readonly property alias cornerSourceItem: heading
    readonly property Item navigationContentItem: list

    color: Theme.surfaceColor(Theme.colors.background.panel)
    clip: true

    PanelHeader {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        title: qsTrId("qml.chart")
        sidebarTitle: true
        showMore: false
    }

    Flickable {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        contentHeight: list.y + list.implicitHeight + 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: list
            x: 6
            y: Theme.workspaceSectionTopMargin
            width: parent.width - 12
            spacing: 2

            NavRow {
                width: parent.width
                text: qsTrId("dialog.unsaved_field_changes.field.metadata")
                iconSource: Qt.resolvedUrl("icons/metadata.svg")
                filledIconSource: Qt.resolvedUrl("icons/metadata-fill.svg")
                selected: root.viewState.metadataEditorActive
                onClicked: {
                    if (root.pages && root.pages.overlayActive)
                        root.pages.leaveOverlayPage()
                    root.viewState.activeSidebarView = "chart"
                    root.viewState.openMetadataEditor()
                }
            }

            NavRow {
                width: parent.width
                text: qsTrId("qml.latency_calibration")
                iconSource: Qt.resolvedUrl("icons/metronome.svg")
                filledIconSource: Qt.resolvedUrl("icons/metronome-fill.svg")
                selected: root.viewState.latencyEditorActive
                onClicked: root.pages.openLatencyPage()
            }

            DifficultyList {
                width: parent.width
                viewState: root.viewState
                documentSession: root.documentSession
                commands: root.commands
            }
        }

        ScrollBar.vertical: AppScrollBar {}
    }
}
