import QtQuick
import MiaCode.UI

AppMenu {
    id: root

    property bool documentAvailable: true
    signal toolRequested(string toolId)

    title: qsTrId("media_tools.video_processing_pv")

    AppMenuAction {
        text: qsTrId("media_tools.prepend_pv_black_screen")
        enabled: root.documentAvailable
        onTriggered: root.toolRequested("prependPv")
    }
    AppMenuAction {
        text: qsTrId("media_tools.align_pv_to_audio")
        enabled: root.documentAvailable
        onTriggered: root.toolRequested("alignPvToAudio")
    }
    AppMenuAction {
        text: qsTrId("media_tools.pv_compress")
        enabled: root.documentAvailable
        onTriggered: root.toolRequested("compressVideo")
    }
    AppMenuAction {
        text: qsTrId("media_tools.batch_compress")
        enabled: root.documentAvailable
        onTriggered: root.toolRequested("batchCompress")
    }
}
