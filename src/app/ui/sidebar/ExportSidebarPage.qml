import QtQuick
import QtQuick.Controls
import MiaCode.UI

Rectangle {
    id: root

    required property var pages
    property bool documentAvailable: true
    readonly property alias cornerSourceItem: heading

    color: Theme.surfaceColor(Theme.colors.background.panel)
    clip: true

    PanelHeader {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        title: qsTrId("sidebar.export")
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
                enabled: root.documentAvailable
                text: qsTrId("export_page.export_video")
                iconSource: Qt.resolvedUrl("icons/video.svg")
                filledIconSource: Qt.resolvedUrl("icons/video-fill.svg")
                selected: root.pages.activePageId === "export"
                onClicked: root.pages.openVideoExportPage()
            }
            NavRow {
                width: parent.width
                enabled: root.documentAvailable
                text: qsTrId("export_page.export_cover")
                iconSource: Qt.resolvedUrl("icons/image.svg")
                filledIconSource: Qt.resolvedUrl("icons/image-fill.svg")
                onClicked: root.pages.openCoverExport()
            }
            NavRow {
                width: parent.width
                enabled: root.documentAvailable
                text: qsTrId("export_page.pack_as_zip")
                iconSource: Qt.resolvedUrl("icons/folder-zip.svg")
                filledIconSource: Qt.resolvedUrl("icons/folder-zip-fill.svg")
                onClicked: root.pages.packAsZip()
            }
        }

        ScrollBar.vertical: AppScrollBar {}
    }
}
