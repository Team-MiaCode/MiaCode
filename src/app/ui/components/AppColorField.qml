import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// Hex field plus a swatch that opens a picker: saturation/value square, hue
// strip and a row of presets. Typed values commit on editingFinished; picker
// drags emit `edited` continuously, the same live path a LabeledSlider uses.
RowLayout {
    id: root

    property string color: "#FFFFFF"
    property var presets: ["#FFFFFF", "#000000", "#808080", "#FF4D6D", "#FF9F1C",
                           "#FFD60A", "#4CD964", "#2EC4F0", "#3D7BFF", "#A259FF"]
    signal edited(string color)

    Layout.fillWidth: true
    spacing: 6

    function normalized(value) {
        return Qt.color(value).toString().toUpperCase()
    }

    AppTextField {
        id: hexField
        Layout.fillWidth: true
        text: root.color
        onEditingFinished: root.edited(text)
    }

    AbstractButton {
        id: swatch
        objectName: "colorFieldSwatch"
        implicitWidth: 22
        implicitHeight: 22
        hoverEnabled: true
        Accessible.name: qsTrId("color_picker.open")
        onClicked: picker.visible ? picker.close() : picker.openFor(swatch)

        Tooltip {
            visible: swatch.hovered && !picker.visible
            text: qsTrId("color_picker.open")
        }

        background: Rectangle {
            radius: Theme.controlRadius
            color: root.color
            border.width: Theme.controlBorderWidth
            border.color: swatch.hovered || picker.visible ? Theme.colors.accent.primary
                                                           : Theme.colors.border.control
        }
    }

    AppStickyPopup {
        id: picker

        readonly property int areaWidth: 200
        // Below this saturation or value a color reads as gray/black.
        readonly property real minimumVisible: 0.2
        property real hue: 0
        property real saturation: 0
        property real value: 1

        function openFor(anchor) {
            const source = Qt.color(root.color)
            // Grays report hue -1; keep the last hue so the strip does not jump.
            if (source.hsvHue >= 0)
                hue = source.hsvHue
            saturation = source.hsvSaturation
            value = source.hsvValue
            const below = anchor.mapToItem(null, 0, anchor.height).y
            openAbove = anchor.Window.height > 0
                        && below + implicitHeight > anchor.Window.height
            openAt(anchor)
        }

        function emitPicked() {
            root.edited(root.normalized(Qt.hsva(hue, saturation, value, 1)))
        }

        padding: 10

        contentItem: Column {
            spacing: 8

            // Saturation runs left to right, value top to bottom.
            Rectangle {
                id: svArea
                implicitWidth: picker.areaWidth
                implicitHeight: 140
                radius: Theme.controlRadius
                color: Qt.hsva(picker.hue, 1, 1, 1)

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#FFFFFFFF" }
                        GradientStop { position: 1; color: "#00FFFFFF" }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    border.width: 1 / Screen.devicePixelRatio
                    border.color: Theme.colors.border.control
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#00000000" }
                        GradientStop { position: 1; color: "#FF000000" }
                    }
                }
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    x: picker.saturation * svArea.width - width / 2
                    y: (1 - picker.value) * svArea.height - height / 2
                    color: "transparent"
                    border.width: 2
                    border.color: picker.value > 0.5 && picker.saturation < 0.5 ? "#000000" : "#FFFFFF"
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    function pick(mouse) {
                        picker.saturation = Math.max(0, Math.min(1, mouse.x / width))
                        picker.value = 1 - Math.max(0, Math.min(1, mouse.y / height))
                        picker.emitPicked()
                    }
                    onPressed: mouse => pick(mouse)
                    onPositionChanged: mouse => { if (pressed) pick(mouse) }
                }
            }

            Rectangle {
                id: hueStrip
                implicitWidth: picker.areaWidth
                implicitHeight: 12
                radius: height / 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0 / 6; color: "#FF0000" }
                    GradientStop { position: 1 / 6; color: "#FFFF00" }
                    GradientStop { position: 2 / 6; color: "#00FF00" }
                    GradientStop { position: 3 / 6; color: "#00FFFF" }
                    GradientStop { position: 4 / 6; color: "#0000FF" }
                    GradientStop { position: 5 / 6; color: "#FF00FF" }
                    GradientStop { position: 6 / 6; color: "#FF0000" }
                }
                Rectangle {
                    width: 6
                    height: parent.height + 4
                    radius: 3
                    x: picker.hue * hueStrip.width - width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#FFFFFF"
                    border.width: 1
                    border.color: "#80000000"
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.topMargin: -4
                    anchors.bottomMargin: -4
                    preventStealing: true
                    function pick(mouse) {
                        // Stop just short of 1 so the far end stays red, not hue -1.
                        picker.hue = Math.max(0, Math.min(0.9999, mouse.x / width))
                        // On white, black or gray a hue change is invisible (the
                        // defaults are #FFFFFF / #000000), so lift the color to
                        // where the chosen hue actually shows.
                        if (picker.saturation < picker.minimumVisible)
                            picker.saturation = 1
                        if (picker.value < picker.minimumVisible)
                            picker.value = 1
                        picker.emitPicked()
                    }
                    onPressed: mouse => pick(mouse)
                    onPositionChanged: mouse => { if (pressed) pick(mouse) }
                }
            }

            Row {
                spacing: (picker.areaWidth - root.presets.length * 16) / Math.max(1, root.presets.length - 1)
                Repeater {
                    model: root.presets
                    AbstractButton {
                        id: preset
                        required property string modelData
                        width: 16
                        height: 16
                        hoverEnabled: true
                        Accessible.name: modelData
                        onClicked: {
                            const picked = Qt.color(modelData)
                            if (picked.hsvHue >= 0)
                                picker.hue = picked.hsvHue
                            picker.saturation = picked.hsvSaturation
                            picker.value = picked.hsvValue
                            root.edited(root.normalized(modelData))
                        }
                        background: Rectangle {
                            radius: Theme.controlRadius
                            color: preset.modelData
                            border.width: preset.hovered ? Theme.controlBorderWidth : 1 / Screen.devicePixelRatio
                            border.color: preset.hovered ? Theme.colors.accent.primary : Theme.colors.border.control
                        }
                    }
                }
            }
        }
    }
}
