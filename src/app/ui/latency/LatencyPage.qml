import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Rectangle {
    id: root

    required property var latency
    required property var pages

    color: Theme.surfaceColor(Theme.colors.background.panel)
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
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        wrapMode: Text.WordWrap
    }

    Flickable {
        id: pageFlick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        contentWidth: width
        contentHeight: form.y + form.implicitHeight + Theme.dialogPadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout {
            id: form
            x: Theme.dialogPadding
            y: Theme.dialogPadding
            width: Math.max(0, pageFlick.width - 2 * Theme.dialogPadding)
            spacing: Theme.settingsRowSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.settingsRowSpacing

                FormLabel { text: qsTrId("qml.bpm") }
                AppTextField {
                    objectName: "latencyBpmField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.latency.bpm.toFixed(3)
                    onEditingFinished: {
                        const parsed = parseFloat(text)
                        if (!isNaN(parsed) && parsed > 0)
                            root.latency.bpm = parsed
                        text = root.latency.bpm.toFixed(3)
                    }
                }
                AppButton {
                    objectName: "latencyDetectBpmButton"
                    text: qsTrId("latency.auto_detect")
                    enabled: root.latency.trackAvailable
                    onClicked: root.latency.detectBpm()
                }
            }
            DetectResult {
                Layout.leftMargin: root.labelWidth + Theme.settingsRowSpacing
                visible: text.length > 0
                text: root.latency.bpmDetectResult
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.settingsRowSpacing

                FormLabel { text: qsTrId("qml.count_in_beats") }
                AppTextField {
                    objectName: "latencyClockCountField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: String(root.latency.clockCount)
                    onEditingFinished: {
                        const parsed = parseInt(text)
                        if (!isNaN(parsed) && parsed > 0)
                            root.latency.clockCount = parsed
                        text = String(root.latency.clockCount)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.settingsRowSpacing

                FormLabel { text: qsTrId("latency.offset") }
                AppTextField {
                    objectName: "latencyOffsetField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.latency.offsetSeconds.toFixed(3)
                    onEditingFinished: {
                        const parsed = parseFloat(text)
                        if (!isNaN(parsed))
                            root.latency.offsetSeconds = parsed
                        text = root.latency.offsetSeconds.toFixed(3)
                    }
                }
                AppButton {
                    objectName: "latencyDetectOffsetButton"
                    text: qsTrId("latency.auto_detect")
                    enabled: root.latency.trackAvailable
                    onClicked: root.latency.detectOffset()
                }
            }
            DetectResult {
                Layout.leftMargin: root.labelWidth + Theme.settingsRowSpacing
                visible: text.length > 0
                text: root.latency.offsetDetectResult
            }

            // The playhead readout already sits in the preview panel's transport,
            // which drives this same audition, so the section does not repeat it.
            SettingsSection {
                objectName: "latencyAuditionCard"
                Layout.fillWidth: true
                title: qsTrId("dialog.render_settings.music.audition")

                ButtonGroup { id: subdivisionGroup }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.settingsRowSpacing

                    FormLabel {
                        text: qsTrId("qml.subdivision")
                    }
                    AppChoiceButton {
                        ButtonGroup.group: subdivisionGroup
                        text: "1/4"
                        checked: root.latency.subdivision === 4
                        onClicked: root.latency.subdivision = 4
                    }
                    AppChoiceButton {
                        ButtonGroup.group: subdivisionGroup
                        text: "1/8"
                        checked: root.latency.subdivision === 8
                        onClicked: root.latency.subdivision = 8
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        objectName: "latencyAuditionButton"
                        emphasized: !root.latency.auditionRunning
                        text: root.latency.auditionRunning ? qsTrId("preview.pause") : qsTrId("qml.start_audition")
                        onClicked: root.latency.toggleAudition()
                    }
                }

                LabeledSlider {
                    objectName: "latencySfxVolumeSlider"
                    label: qsTrId("qml.sound_effect_volume")
                    labelWidth: root.labelWidth
                    from: 0
                    to: 100
                    stepSize: 1
                    value: root.latency.sfxVolumePercent
                    onMoved: function(value) { root.latency.sfxVolumePercent = Math.round(value) }
                }
            }
        }

        ScrollBar.vertical: AppScrollBar {}
    }
}
