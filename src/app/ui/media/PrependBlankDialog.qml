import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// Beats/BPM for the blank prepended to track.mp3 or the background video.
AppDialog {
    id: root

    required property var mediaTools

    property bool isTrack: true
    title: isTrack ? qsTrId("media_tools.prepend_track_silence")
                   : qsTrId("media_tools.prepend_pv_black_screen")
    property string inputName: ""
    property string backupName: ""
    property bool hasBackup: false

    readonly property real beats: beatsField.value
    readonly property real bpm: bpmField.value
    readonly property real blankSeconds: bpm > 0 ? beats * 60 / bpm : 0

    preferredWidth: 440
    preferredHeight: Math.max(240, implicitHeight)
    footer: DialogFooter {
        acceptText: qsTrId("action.ok")
        cancelText: qsTrId("qml.back")
        onAccepted: root.accept()
        onRejected: root.reject()
    }

    function loadContext(context) {
        root.isTrack = context.isTrack
        root.inputName = context.inputName
        root.backupName = context.backupName
        root.hasBackup = context.hasBackup
        beatsField.value = context.beats
        bpmField.value = context.bpm
    }

    function formatNumber(value) { return Number(value.toFixed(3)).toString() }

    onAccepted: root.mediaTools.applyPrepend(root.isTrack, root.beats, root.bpm)

    body: ColumnLayout {
        spacing: Theme.settingsRowSpacing

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: summaryLabel.implicitHeight + 16
            radius: Theme.controlRadius
            color: Theme.overlayColor(Theme.colors.background.control)
            border.width: 1 / Screen.devicePixelRatio
            border.color: Theme.colors.border.normal

            Text {
                id: summaryLabel
                objectName: "prependBlankSummary"
                anchors.fill: parent
                anchors.margins: 8
                text: (root.isTrack
                    ? qsTrId("qml.insert_2_beats_of_silence_bpm_3_about_4_seconds_at_the_beginning")
                    : qsTrId("qml.insert_2_beats_of_black_screen_bpm_3_about_4_seconds_at_the_begi"))
                    .arg(root.inputName)
                    .arg(root.formatNumber(root.beats))
                    .arg(root.formatNumber(root.bpm))
                    .arg(root.formatNumber(root.blankSeconds))
                color: Theme.colors.text.primary
                font.family: Theme.uiFont
                font.pixelSize: Theme.secondaryFontSize
                wrapMode: Text.WordWrap
            }
        }

        component NumberRow: RowLayout {
            id: numberRow
            required property string label
            property alias value: spin.realValue
            property real minimum: 0
            property real maximum: 1000
            signal detected(var values)
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.preferredWidth: 68
                text: numberRow.label
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
            }
            AppTextField {
                id: spin
                property real realValue: 0
                Layout.fillWidth: true
                text: root.formatNumber(spin.realValue)
                onEditingFinished: {
                    const parsed = parseFloat(text)
                    if (!isNaN(parsed))
                        spin.realValue = Math.min(numberRow.maximum, Math.max(numberRow.minimum, parsed))
                    text = root.formatNumber(spin.realValue)
                }
            }
            AppButton {
                minimumButtonWidth: 72
                text: qsTrId("qml.detect")
                onClicked: numberRow.detected(root.mediaTools.detectPrependTiming(root.isTrack))
            }
        }

        NumberRow {
            id: beatsField
            objectName: "prependBlankBeatsRow"
            label: qsTrId("media_tools.beats")
            minimum: 0.125
            maximum: 512
            value: 4
            onDetected: values => { if (values.beats !== undefined) beatsField.value = values.beats }
        }

        NumberRow {
            id: bpmField
            objectName: "prependBlankBpmRow"
            label: "BPM"
            minimum: 1
            maximum: 999
            value: 120
            onDetected: values => { if (values.bpm !== undefined) bpmField.value = values.bpm }
        }

        AppButton {
            objectName: "prependBlankRestoreButton"
            Layout.alignment: Qt.AlignLeft
            visible: root.hasBackup
            text: qsTrId("qml.restore_backup_1").arg(root.backupName)
            onClicked: {
                root.close()
                root.mediaTools.restorePrependBackup(root.isTrack)
            }
        }
    }
}
