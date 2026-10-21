import QtQuick
import QtQuick.Layouts
import MiaCode.UI

// Single launcher for the media tools that act on the current chart. Each row
// closes the launcher before starting its tool, so the tool's own dialog or
// notice has the window to itself.
AppDialog {
    id: root

    required property var mediaTools
    property bool documentAvailable: true

    title: qsTrId("media_tools.audio_video_processing")
    preferredWidth: 560
    preferredHeight: implicitHeight
    footer: DialogFooter {
        cancelText: qsTrId("action.close")
        onRejected: root.reject()
    }

    signal prependRequested(bool isTrack)
    signal batchCompressRequested()

    body: ColumnLayout {
        spacing: 4

        component ToolRow: ChromeRow {
            id: toolRow
            required property string label
            required property string description
            Layout.fillWidth: true
            implicitHeight: rowLayout.implicitHeight + 16
            leftPadding: 12
            rightPadding: 12
            contentItem: ColumnLayout {
                id: rowLayout
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: toolRow.label
                    color: toolRow.enabled ? Theme.colors.text.active
                                          : Theme.colors.text.disabled
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    text: toolRow.description
                    color: toolRow.enabled ? Theme.colors.text.secondary
                                          : Theme.colors.text.disabled
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.secondaryFontSize
                    wrapMode: Text.WordWrap
                }
            }
        }

        ToolRow {
            objectName: "mediaToolPrependTrack"
            enabled: root.documentAvailable
            label: qsTrId("media_tools.prepend_blank")
            description: qsTrId("qml.insert_blank_audio_of_the_specified_duration_at_the_beginning_of")
            onClicked: {
                root.close()
                root.prependRequested(true)
            }
        }
        ToolRow {
            objectName: "mediaToolSampleRate"
            enabled: root.documentAvailable
            label: qsTrId("media_tools.sample_rate_conversion")
            description: qsTrId("qml.convert_the_sample_rate_of_track_mp3_to_44100_hz_and_automatical")
            onClicked: {
                root.close()
                root.mediaTools.convertTrackTo44100Hz()
            }
        }
        ToolRow {
            objectName: "mediaToolPrependPv"
            enabled: root.documentAvailable
            label: qsTrId("media_tools.prepend_pv_black_screen")
            description: qsTrId("qml.insert_a_black_screen_of_the_specified_duration_at_the_beginning")
            onClicked: {
                root.close()
                root.prependRequested(false)
            }
        }
        ToolRow {
            objectName: "mediaToolAlignPv"
            enabled: root.documentAvailable
            label: qsTrId("media_tools.align_pv_to_audio")
            description: qsTrId("media_tools.align_pv_to_audio_description")
            onClicked: {
                root.close()
                root.mediaTools.alignBackgroundVideoToTrack()
            }
        }
        ToolRow {
            objectName: "mediaToolCompressVideo"
            enabled: root.documentAvailable
            label: qsTrId("media_tools.pv_compress")
            description: qsTrId("media_tools.compress_the_background_video_under")
            onClicked: {
                root.close()
                root.mediaTools.compressBackgroundVideo()
            }
        }
        ToolRow {
            objectName: "mediaToolBatchPv"
            enabled: root.documentAvailable
            label: qsTrId("media_tools.batch_compress")
            description: qsTrId("media_tools.batch_pv_description")
            onClicked: {
                root.close()
                root.batchCompressRequested()
            }
        }
    }
}
