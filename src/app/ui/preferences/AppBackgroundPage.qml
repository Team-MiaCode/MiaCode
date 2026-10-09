import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// Shared v2 background page; each control writes through to AppBackgroundModel.
ColumnLayout {
    id: root
    required property var appBackground
    objectName: "preferencesBackgroundPage"
    Layout.fillWidth: true
    spacing: 10

    AppSwitch {
        text: qsTrId("qml.enable_application_background")
        checked: root.appBackground.enabled
        onToggled: root.appBackground.enabled = checked
    }
    RowLayout {
        Layout.fillWidth: true
        Text {
            Layout.fillWidth: true
            text: root.appBackground.imagePath.length > 0
                  ? root.appBackground.imagePath
                  : qsTrId("qml.no_background_image_selected")
            color: root.appBackground.imageReadable
                   ? Theme.colors.text.primary : Theme.colors.text.secondary
            elide: Text.ElideMiddle
            font.family: Theme.uiFont
        }
        AppButton {
            text: qsTrId("cover.choose_image")
            onClicked: root.appBackground.chooseImage()
        }
        AppButton {
            text: qsTrId("dialog.preferences.background.clear")
            enabled: root.appBackground.imagePath.length > 0
            onClicked: root.appBackground.clearImage()
        }
    }
    Text {
        Layout.fillWidth: true
        visible: root.appBackground.errorMessage.length > 0
        text: root.appBackground.errorMessage
        color: Theme.colors.syntax.error
        font.family: Theme.uiFont
        wrapMode: Text.WordWrap
    }
    LabeledSlider {
        objectName: "preferencesBackgroundOpacitySlider"
        label: qsTrId("qml.image_opacity")
        from: 0.1; to: 0.8; stepSize: 0.01
        value: root.appBackground.opacity
        readout: Math.round(root.appBackground.opacity * 100) + "%"
        onMoved: function(value) { root.appBackground.opacity = value }
    }
    LabeledSlider {
        objectName: "preferencesBackgroundPanelSlider"
        label: qsTrId("qml.background_mask_opacity")
        from: 0; to: 1; stepSize: 0.01
        value: root.appBackground.panelAlpha / 255.0
        readout: Math.round(root.appBackground.panelAlpha / 255.0 * 100) + "%"
        onMoved: function(value) { root.appBackground.panelAlpha = Math.round(value * 255) }
    }
    LabeledSlider {
        objectName: "preferencesBackgroundBlurSlider"
        label: qsTrId("qml.blur_radius")
        from: 0; to: 64; stepSize: 1
        value: root.appBackground.blur
        readout: Math.round(root.appBackground.blur)
        onMoved: function(value) { root.appBackground.blur = Math.round(value) }
    }
    LabeledCombo {
        objectName: "preferencesBackgroundScaleCombo"
        label: qsTrId("qml.scale_mode")
        options: [
            { value: "cover", label: qsTrId("dialog.preferences.background.scale.cover") },
            { value: "contain", label: qsTrId("dialog.preferences.background.scale.contain") },
            { value: "stretch", label: qsTrId("dialog.preferences.background.scale.stretch") },
            { value: "center", label: qsTrId("dialog.preferences.background.scale.center") },
            { value: "repeat", label: qsTrId("dialog.preferences.background.scale.repeat") }
        ]
        currentValue: root.appBackground.sizeMode
        onPicked: function(value) { root.appBackground.sizeMode = value }
    }
    LabeledCombo {
        objectName: "preferencesBackgroundPositionCombo"
        label: qsTrId("dialog.preferences.background.position")
        options: [
            { value: "center", label: qsTrId("dialog.preferences.background.position.center") },
            { value: "left", label: qsTrId("dialog.preferences.background.position.left") },
            { value: "right", label: qsTrId("dialog.preferences.background.position.right") },
            { value: "top", label: qsTrId("dialog.preferences.background.position.top") },
            { value: "bottom", label: qsTrId("dialog.preferences.background.position.bottom") },
            { value: "left_top", label: qsTrId("dialog.preferences.background.position.left_top") },
            { value: "right_top", label: qsTrId("dialog.preferences.background.position.right_top") },
            { value: "left_bottom", label: qsTrId("dialog.preferences.background.position.left_bottom") },
            { value: "right_bottom", label: qsTrId("dialog.preferences.background.position.right_bottom") }
        ]
        currentValue: root.appBackground.position
        onPicked: function(value) { root.appBackground.position = value }
    }

}
