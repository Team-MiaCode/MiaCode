import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    required property var latency
    required property var previewSession
    required property var pages

    clip: true
    readonly property int labelWidth: 120

    component FormLabel: Text {
        Layout.preferredWidth: root.labelWidth
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        wrapMode: Text.WordWrap
    }

    component DetectResult: Text {
        Layout.fillWidth: true
        Layout.leftMargin: root.labelWidth + Theme.settingsRowSpacing
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        wrapMode: Text.WordWrap
    }

    Flickable {
        id: pageFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: form.y + form.implicitHeight + Theme.dialogPadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: form
            x: Theme.dialogPadding
            y: Theme.dialogPadding
            width: Math.max(0, pageFlick.width - 2 * Theme.dialogPadding)
            spacing: Theme.settingsRowSpacing

            SettingsSection {
                Layout.fillWidth: true
                title: ""

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.settingsRowSpacing
                    FormLabel { text: qsTrId("qml.bpm") }
                    AppTextField {
                        objectName: "latencyBpmField"
                        backgroundColor: Theme.colors.background.panel
                        outlined: true
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.latency.bpm.toFixed(3)
                        onEditingFinished: {
                            const parsed = parseFloat(text)
                            if (!isNaN(parsed) && parsed > 0) root.latency.bpm = parsed
                            text = Qt.binding(() => root.latency.bpm.toFixed(3))
                        }
                    }
                    AppButton {
                        objectName: "latencyDetectBpmButton"
                        text: qsTrId("latency.auto_detect")
                        enabled: root.latency.trackAvailable
                        onClicked: root.latency.detectBpm()
                    }
                }
                DetectResult { visible: text.length > 0; text: root.latency.bpmDetectResult }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.settingsRowSpacing
                    FormLabel { text: qsTrId("qml.count_in_beats") }
                    AppTextField {
                        objectName: "latencyClockCountField"
                        backgroundColor: Theme.colors.background.panel
                        outlined: true
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: String(root.latency.clockCount)
                        onEditingFinished: {
                            const parsed = parseInt(text)
                            if (!isNaN(parsed) && parsed > 0) root.latency.clockCount = parsed
                            text = Qt.binding(() => String(root.latency.clockCount))
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.settingsRowSpacing
                    FormLabel { text: qsTrId("latency.offset") }
                    AppTextField {
                        objectName: "latencyOffsetField"
                        backgroundColor: Theme.colors.background.panel
                        outlined: true
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.latency.offsetSeconds.toFixed(3)
                        onEditingFinished: {
                            const parsed = parseFloat(text)
                            if (!isNaN(parsed)) root.latency.offsetSeconds = parsed
                            text = Qt.binding(() => root.latency.offsetSeconds.toFixed(3))
                        }
                    }
                    AppButton {
                        objectName: "latencyDetectOffsetButton"
                        text: qsTrId("latency.auto_detect")
                        enabled: root.latency.trackAvailable
                        onClicked: root.latency.detectOffset()
                    }
                }
                DetectResult { visible: text.length > 0; text: root.latency.offsetDetectResult }
            }

            SettingsSection {
                objectName: "latencyAuditionCard"
                Layout.fillWidth: true
                title: ""

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.settingsRowSpacing
                    FormLabel { text: qsTrId("qml.subdivision") }
                    AppSegmentedControl {
                        Layout.preferredWidth: 210
                        tabs: [
                            { id: "4", label: "1/4" },
                            { id: "8", label: "1/8" },
                            { id: "16", label: "1/16" }
                        ]
                        selectedId: String(root.latency.subdivision)
                        onTabSelected: id => root.latency.subdivision = Number(id)
                    }
                    Item { Layout.fillWidth: true }
                }

                LabeledSlider {
                    objectName: "latencySfxVolumeSlider"
                    label: qsTrId("qml.sound_effect_volume")
                    labelWidth: root.labelWidth
                    from: 0
                    to: 100
                    stepSize: 1
                    value: root.latency.sfxVolumePercent
                    onMoved: value => root.latency.sfxVolumePercent = Math.round(value)
                }

                AppButton {
                    objectName: "latencyAuditionButton"
                    Layout.alignment: Qt.AlignRight
                    emphasized: !root.previewSession.playing
                    text: root.previewSession.playing ? qsTrId("preview.pause") : qsTrId("qml.start_audition")
                    onClicked: root.previewSession.playing = !root.previewSession.playing
                }
            }
        }
    }
}
