import QtQuick
import MiaCode.UI

AppMenu {
    id: root

    property bool documentAvailable: true
    signal toolRequested(string toolId)

    title: qsTrId("media_tools.audio_processing_track")

    AppMenuAction {
        text: qsTrId("media_tools.prepend_blank")
        enabled: root.documentAvailable
        onTriggered: root.toolRequested("prependTrack")
    }
    AppMenuAction {
        text: qsTrId("media_tools.sample_rate_conversion")
        enabled: root.documentAvailable
        onTriggered: root.toolRequested("convertTrack")
    }
}
