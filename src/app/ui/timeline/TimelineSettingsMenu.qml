import QtQuick
import QtQuick.Controls
import MiaCode.UI

AppStickyPopup {
    id: root

    required property var stateBridge
    required property var timelineSession
    openAbove: true

    readonly property int brightnessPercentMin: 20
    readonly property int brightnessPercentMax: 200
    readonly property int brightnessPercentStep: 5

    function percentFromBrightness(value) {
        const percent = Math.round(value * 100 / brightnessPercentStep) * brightnessPercentStep
        return Math.max(brightnessPercentMin, Math.min(brightnessPercentMax, percent))
    }

    component ParameterItem: Item {
        id: row

        required property string text
        required property string valueText
        required property real from
        required property real to
        required property real stepSize
        required property real value
        signal edited(real value)

        implicitWidth: Math.max(188, 12 + titleMetrics.advanceWidth
                                + 8 + valueMetrics.advanceWidth + 16)
        implicitHeight: Theme.menuParameterRowHeight

        TextMetrics {
            id: titleMetrics
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            text: row.text
        }

        TextMetrics {
            id: valueMetrics
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            text: valueLabel.text
        }

        Column {
            id: body
            x: 12
            y: 4
            width: Math.max(0, row.width - 12 - 16)
            spacing: 0

            Item {
                width: parent.width
                implicitHeight: Theme.menuRowHeight

                Text {
                    id: titleLabel
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: valueLabel.left
                    anchors.rightMargin: 8
                    text: row.text
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    color: Theme.colors.text.primary
                    elide: Text.ElideRight
                }

                Text {
                    id: valueLabel
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.valueText
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    color: Theme.colors.text.secondary
                    horizontalAlignment: Text.AlignRight
                }
            }

            AppSlider {
                id: slider
                implicitHeight: Theme.menuRowHeight
                width: parent.width
                from: row.from
                to: row.to
                stepSize: row.stepSize
                snapMode: Slider.SnapAlways
                value: row.value
                Accessible.name: row.text
                onMoved: row.edited(value)
                onPressedChanged: if (!pressed) value = row.value

                Connections {
                    target: row
                    function onValueChanged() {
                        if (!slider.pressed)
                            slider.value = row.value
                    }
                }
            }
        }
    }

    contentItem: Column {
        spacing: 0

        ParameterItem {
            width: parent.width
            text: qsTrId("qml.timeline_zoom")
            readonly property var presets: root.stateBridge ? root.stateBridge.zoomPresetValues : []
            valueText: qsTrId("qml.1").arg(Math.round(root.stateBridge
                ? root.stateBridge.zoomScale * 100 : 50))
            from: 0
            to: Math.max(0, presets.length - 1)
            stepSize: 1
            value: {
                for (let index = 0; index < presets.length; ++index) {
                    if (Math.abs(presets[index] - root.stateBridge.zoomScale) <= 1e-6)
                        return index
                }
                return 0
            }
            onEdited: value => root.stateBridge.applyZoomPreset(presets[Math.round(value)])
        }

        ParameterItem {
            width: parent.width
            text: qsTrId("shell.timeline_waveform_brightness")
            readonly property real brightness: root.stateBridge ? root.stateBridge.waveformBrightness : 0.5
            valueText: qsTrId("qml.1").arg(Math.round(brightness * 100))
            from: root.brightnessPercentMin
            to: root.brightnessPercentMax
            stepSize: root.brightnessPercentStep
            value: root.percentFromBrightness(brightness)
            onEdited: value => root.stateBridge.waveformBrightness = value / 100
        }

        ParameterItem {
            width: parent.width
            text: qsTrId("shell.timeline_measure_line_brightness")
            readonly property real brightness: root.stateBridge ? root.stateBridge.measureLineBrightness : 1.0
            valueText: qsTrId("qml.1").arg(Math.round(brightness * 100))
            from: root.brightnessPercentMin
            to: root.brightnessPercentMax
            stepSize: root.brightnessPercentStep
            value: root.percentFromBrightness(brightness)
            onEdited: value => root.stateBridge.measureLineBrightness = value / 100
        }

        AppMenuSeparator { width: parent.width }

        AppSwitch {
            id: followSwitch
            width: parent.width
            implicitHeight: Theme.menuRowHeight
            leftPadding: 12
            rightPadding: 16
            text: root.timelineSession.followCodeLabel
            checked: root.stateBridge && root.stateBridge.followPreviewEnabled
            onToggled: {
                if (root.stateBridge && checked !== root.stateBridge.followPreviewEnabled)
                    root.timelineSession.followPreviewToggled(checked)
            }
            Connections {
                target: root.stateBridge
                function onFollowPreviewEnabledChanged() {
                    followSwitch.checked = root.stateBridge.followPreviewEnabled
                }
            }
        }
    }
}
