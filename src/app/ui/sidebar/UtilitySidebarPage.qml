pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import MiaCode.UI

Rectangle {
    id: root

    required property string mode
    required property var pages
    property bool documentAvailable: true
    readonly property alias cornerSourceItem: heading
    signal toolRequested(string toolId)

    color: Theme.surfaceColor(Theme.colors.background.panel)
    clip: true

    component ToolSectionHeader: Text {
        width: parent.width
        height: 30
        leftPadding: 8
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        font.bold: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component ToolRow: NavRow {
        width: parent.width
        textLeftPadding: 20
        enabled: root.documentAvailable
    }

    PanelHeader {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        title: root.mode === "network" ? qsTrId("net.ui.majnet") : qsTrId("qml.tools")
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
        ScrollBar.vertical: AppScrollBar {}

        Column {
            id: list
            x: 6
            y: Theme.workspaceSectionTopMargin
            width: parent.width - 12
            spacing: 2

            NavRow {
                width: parent.width
                visible: root.mode === "network"
                text: qsTrId("net.ui.download_page")
                iconSource: Qt.resolvedUrl("icons/download.svg")
                filledIconSource: Qt.resolvedUrl("icons/download-fill.svg")
                selected: root.pages.activePageId === "net-download"
                onClicked: root.pages.openNetPage("net-download")
            }
            NavRow {
                width: parent.width
                visible: root.mode === "network" && root.pages.netUploadEnabled
                text: qsTrId("net.ui.upload_page")
                iconSource: Qt.resolvedUrl("icons/upload.svg")
                filledIconSource: Qt.resolvedUrl("icons/upload-fill.svg")
                selected: root.pages.activePageId === "net-upload"
                onClicked: root.pages.openNetPage("net-upload")
            }

            Column {
                width: parent.width
                visible: root.mode === "tools"
                spacing: 2

                ToolSectionHeader {
                    text: qsTrId("media_tools.audio_processing_track")
                }
                ToolRow {
                    text: qsTrId("media_tools.prepend_blank")
                    onClicked: root.toolRequested("prependTrack")
                }
                ToolRow {
                    text: qsTrId("media_tools.sample_rate_conversion")
                    onClicked: root.toolRequested("convertTrack")
                }
                ToolSectionHeader {
                    height: 38
                    topPadding: 8
                    text: qsTrId("media_tools.video_processing_pv")
                }
                ToolRow {
                    text: qsTrId("media_tools.prepend_pv_black_screen")
                    onClicked: root.toolRequested("prependPv")
                }
                ToolRow {
                    text: qsTrId("media_tools.align_pv_to_audio")
                    onClicked: root.toolRequested("alignPvToAudio")
                }
                ToolRow {
                    text: qsTrId("media_tools.pv_compress")
                    onClicked: root.toolRequested("compressVideo")
                }
                ToolRow {
                    text: qsTrId("media_tools.batch_compress")
                    onClicked: root.toolRequested("batchCompress")
                }
            }
        }
    }
}
