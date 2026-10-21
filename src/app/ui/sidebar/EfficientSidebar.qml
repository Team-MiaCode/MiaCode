pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import MiaCode.UI

Rectangle {
    id: root

    required property var viewState
    required property var documentSession
    required property var commands
    required property var pages
    property bool toolsMenuActive: false
    property bool nativeMaterialActive: Theme.nativeMaterialActive
    readonly property var navigationStateColors: nativeMaterialActive
        ? Theme.compensatedStateColorsFor(Theme.colors.listState) : Theme.colors.listState
    readonly property Item cornerSourceItem: heading
    readonly property Item navigationContentItem: difficulties
    readonly property Item toolsItem: toolsRow

    signal toolsRequested()
    signal settingsRequested()

    function showTools() {
        scroll.contentY = Math.max(0, Math.min(toolsRow.y,
            scroll.contentHeight - scroll.height))
        root.toolsRequested()
    }

    color: Theme.chromeSurfaceColor(Theme.colors.background.panel, root.nativeMaterialActive)
    clip: true

    PanelHeader {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        title: qsTrId("sidebar.menu")
        sidebarTitle: true
    }

    Flickable {
        id: scroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: settingsRow.top
        contentHeight: contents.implicitHeight + 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        Column {
            id: contents
            x: 6
            y: 6
            width: parent.width - 12
            spacing: 6

            SidebarRow {
                id: metadataRow
                width: parent.width
                text: qsTrId("dialog.unsaved_field_changes.field.metadata")
                iconSource: Qt.resolvedUrl("icons/metadata.svg")
                filledIconSource: Qt.resolvedUrl("icons/metadata-fill.svg")
                enabled: root.documentSession.hasDocument
                selected: root.viewState.metadataEditorActive && !root.pages.overlayActive
                onClicked: {
                    if (root.pages.overlayActive)
                        root.pages.leaveOverlayPage()
                    root.viewState.openMetadataEditor()
                }
            }

            SidebarRow {
                width: parent.width
                text: qsTrId("qml.latency_calibration")
                iconSource: Qt.resolvedUrl("icons/metronome.svg")
                filledIconSource: Qt.resolvedUrl("icons/metronome-fill.svg")
                enabled: root.documentSession.hasDocument
                selected: root.viewState.latencyEditorActive && !root.pages.overlayActive
                onClicked: root.pages.openLatencyPage()
            }

            SectionDivider {}

            DifficultyList {
                id: difficulties
                width: parent.width
                enabled: root.documentSession.hasDocument
                flatNavigation: true
                rowStateColors: root.navigationStateColors
                viewState: root.viewState
                documentSession: root.documentSession
                commands: root.commands
                overlayActive: root.pages.overlayActive
            }

            SectionDivider {}

            SidebarRow {
                width: parent.width
                text: qsTrId("export_page.export_video")
                iconSource: Qt.resolvedUrl("icons/video.svg")
                filledIconSource: Qt.resolvedUrl("icons/video-fill.svg")
                enabled: root.documentSession.hasDocument
                selected: root.pages.activePageId === "export"
                onClicked: {
                    root.pages.rememberEditorReturnTarget(root.viewState.activeEditorKey)
                    root.pages.openVideoExportPage()
                }
            }
            SidebarRow {
                width: parent.width
                text: qsTrId("export_page.export_cover")
                iconSource: Qt.resolvedUrl("icons/image.svg")
                filledIconSource: Qt.resolvedUrl("icons/image-fill.svg")
                enabled: root.documentSession.hasDocument
                onClicked: root.pages.openCoverExport()
            }
            SidebarRow {
                width: parent.width
                text: qsTrId("export_page.pack_as_zip")
                iconSource: Qt.resolvedUrl("icons/folder-zip.svg")
                filledIconSource: Qt.resolvedUrl("icons/folder-zip-fill.svg")
                enabled: root.documentSession.hasDocument
                onClicked: root.pages.packAsZip()
            }
            SidebarRow {
                id: toolsRow
                width: parent.width
                text: qsTrId("qml.tools")
                selected: root.toolsMenuActive
                iconSource: Qt.resolvedUrl("icons/tools.svg")
                filledIconSource: Qt.resolvedUrl("icons/tools-fill.svg")
                onClicked: root.toolsRequested()
            }
        }
    }

    SidebarRow {
        id: settingsRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        text: qsTrId("dialog.preferences.title")
        iconSource: Qt.resolvedUrl("icons/settings.svg")
        onClicked: root.settingsRequested()
    }

    component SidebarRow: NavRow {
        stateColors: root.navigationStateColors
        implicitHeight: 36
        emphasizedText: true
        textPixelSize: Theme.uiFontSize + 1
    }

    component SectionDivider: Item {
        width: parent.width
        height: 9

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            height: 1 / Screen.devicePixelRatio
            color: Theme.separatorColor
        }
    }

}
