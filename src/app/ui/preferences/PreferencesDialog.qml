import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// 偏好设置. Every control writes straight through to the model, which persists
// on each change — there is no OK/Apply, matching the Widgets dialog it
// replaces. Language and theme changes update their presentation through the
// corresponding model notifications.
//
// 快捷键 is a page here, not a dialog of its own: a modal opened from a modal
// stacked two scrims over the same settings and put the key capture behind an
// extra Escape.
AppDialog {
    id: root

    required property var preferencesModel
    required property var shortcuts
    required property var preferences
    required property var appBackground
    property var updateService: null

    signal updateRequested()

    title: qsTrId("dialog.preferences.title")
    preferredWidth: 700
    preferredHeight: Theme.dialogHeight
    fillBody: true
    footer: DialogFooter {
        cancelText: qsTrId("action.close")
        onRejected: root.reject()
    }

    // Id of the command whose binding is being recorded; "" when idle.
    property string capturingId: ""
    property var shortcutRows: []

    onAboutToShow: {
        root.capturingId = ""
        root.refreshShortcuts()
    }

    // Reassigning the model resets ListView.contentY, which threw the reader
    // back to the top of the list after every recorded binding. Restore the
    // scroll position around the rebuild.
    function refreshShortcuts() {
        const keepY = shortcutList.contentY
        root.shortcutRows = root.shortcuts.editableShortcuts()
        shortcutList.contentY = Math.max(
            0, Math.min(keepY, Math.max(0, shortcutList.contentHeight - shortcutList.height)))
    }

    component PreviewPositionCard: AbstractButton {
        id: card

        required property bool previewOnLeft
        required property bool selected
        readonly property string layoutLabel: previewOnLeft
            ? qsTrId("dialog.preferences.layout.preview_editor")
            : qsTrId("dialog.preferences.layout.editor_preview")

        implicitWidth: 180
        implicitHeight: (width - 24) * 10 / 16 + 46
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.role: Accessible.RadioButton
        Accessible.name: qsTrId("dialog.preferences.interface_layout") + ": " + layoutLabel
        Accessible.checkable: true
        Accessible.checked: selected

        background: Rectangle {
            radius: Theme.controlRadius
            color: Theme.overlayColor(card.selected ? Theme.colors.state.selected
                 : card.down ? Theme.colors.state.pressed
                 : card.hovered ? Theme.colors.state.hover : Theme.colors.background.control)
            border.width: card.selected || card.visualFocus ? 2 : 1
            border.color: card.selected || card.visualFocus
                ? Theme.colors.accent.primary : Theme.colors.border.control
        }

        contentItem: Item {
            Rectangle {
                id: miniature
                readonly property real contentLeft: miniatureSidebar.x + miniatureSidebar.width + 4
                readonly property real contentWidth: width - contentLeft - 5
                readonly property real paneSpacing: 8
                readonly property real paneWidth: (contentWidth - paneSpacing) / 2
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                height: width * 10 / 16
                radius: Theme.compactControlRadius
                color: Theme.surfaceColor(Theme.colors.background.surface)
                border.width: 1
                border.color: Theme.colors.border.normal

                Rectangle {
                    x: 5; y: 5
                    width: parent.width - 10
                    height: 5
                    radius: 2
                    color: Theme.colors.border.control
                }
                Rectangle {
                    id: miniatureSidebar
                    x: 5; y: 14
                    width: miniature.width * 0.14
                    height: parent.height - 19
                    radius: 2
                    color: Theme.colors.border.control
                }

                Item {
                    id: previewPane
                    x: card.previewOnLeft ? miniature.contentLeft : miniature.width - width - 5
                    y: 14
                    width: miniature.paneWidth
                    height: miniature.height - 19

                    Item {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: previewControls.top
                        anchors.bottomMargin: 4

                        Rectangle {
                            id: previewCircle
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 8, parent.height - 8)
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 2
                            border.color: Theme.colors.accent.primary
                        }
                    }
                    Grid {
                        id: previewControls
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: previewCircle.width
                        columns: 3
                        spacing: 3

                        Repeater {
                            model: 3
                            delegate: Item {
                                width: (previewControls.width - 6) / 3
                                height: 5

                                Rectangle {
                                    width: 3; height: 3
                                    y: 1
                                    radius: 1
                                    color: Theme.colors.text.disabled
                                }
                                Rectangle {
                                    x: 5; y: 0
                                    width: parent.width - 5
                                    height: 1
                                    color: Theme.colors.text.disabled
                                }
                                Rectangle {
                                    x: 5; y: 4
                                    width: (parent.width - 5) * 0.6
                                    height: 1
                                    color: Theme.colors.text.secondary
                                }
                            }
                        }
                    }
                }

                Item {
                    id: editorPane
                    x: card.previewOnLeft ? miniature.width - width - 5 : miniature.contentLeft
                    y: 14
                    width: miniature.paneWidth
                    height: miniature.height - 19

                    Column {
                        y: 6
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 5
                        Repeater {
                            model: [0.85, 0.6, 0.75, 0.5]
                            delegate: Rectangle {
                                required property real modelData
                                width: editorPane.width * modelData
                                height: 2
                                radius: 1
                                color: Theme.colors.text.disabled
                            }
                        }
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 18
                        radius: 2
                        color: Theme.colors.border.control

                        Repeater {
                            model: 5
                            delegate: Rectangle {
                                required property int index
                                x: (index + 1) * parent.width / 6
                                y: 3
                                width: 1
                                height: parent.height - 6
                                color: Theme.colors.text.disabled
                            }
                        }
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 10
                text: card.layoutLabel
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                color: card.selected ? Theme.colors.text.active : Theme.colors.text.secondary
            }
        }
    }

    body: ColumnLayout {
        spacing: Theme.settingsRowSpacing

        AppTabBar {
            id: preferencesTabs
            Layout.fillWidth: true
            onCurrentIndexChanged: root.capturingId = ""
            tabs: [qsTrId("qml.interface"), qsTrId("dialog.preferences.background_group"), qsTrId("dialog.preferences.editor_group"), qsTrId("dialog.preferences.performance_group"), qsTrId("dialog.preferences.shortcuts_group"), qsTrId("dialog.preferences.updates_group")]
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.colors.border.normal
        }

        AppTabPages {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: preferencesTabs.currentIndex

            // ---- 界面 ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                spacing: Theme.settingsRowSpacing

                LabeledCombo {
                    objectName: "preferencesLanguageCombo"
                    label: qsTrId("dialog.preferences.language")
                    options: root.preferencesModel.languageOptions
                    currentValue: root.preferencesModel.languageToken
                    onPicked: function(value) { root.preferencesModel.languageToken = value }
                }
                LabeledCombo {
                    objectName: "preferencesThemeModeCombo"
                    label: qsTrId("dialog.preferences.theme.mode")
                    options: root.preferencesModel.themeModeOptions
                    currentValue: root.preferencesModel.themeModeToken
                    onPicked: function(value) { root.preferencesModel.themeModeToken = value }
                }
                LabeledCombo {
                    objectName: "preferencesLightThemeCombo"
                    visible: root.preferencesModel.themeModeToken !== "dark"
                    label: qsTrId("dialog.preferences.theme.light_palette")
                    options: root.preferencesModel.themePaletteOptions
                    currentValue: root.preferencesModel.lightThemeToken
                    onPicked: function(value) { root.preferencesModel.lightThemeToken = value }
                }
                LabeledCombo {
                    objectName: "preferencesDarkThemeCombo"
                    visible: root.preferencesModel.themeModeToken !== "light"
                    label: qsTrId("dialog.preferences.theme.dark_palette")
                    options: root.preferencesModel.themePaletteOptions
                    currentValue: root.preferencesModel.darkThemeToken
                    onPicked: function(value) { root.preferencesModel.darkThemeToken = value }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        Layout.preferredWidth: 120
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: Math.round((Theme.controlMinHeight - implicitHeight) / 2)
                        text: qsTrId("dialog.preferences.interface_layout")
                        color: Theme.colors.text.secondary
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.chromeInsetY
                        spacing: 24

                        PreviewPositionCard {
                            objectName: "preferencesPreviewRightCard"
                            Layout.fillWidth: true
                            Layout.maximumWidth: 200
                            previewOnLeft: false
                            selected: !root.preferencesModel.previewOnLeft
                            onClicked: root.preferencesModel.previewOnLeft = false
                        }
                        PreviewPositionCard {
                            objectName: "preferencesPreviewLeftCard"
                            Layout.fillWidth: true
                            Layout.maximumWidth: 200
                            previewOnLeft: true
                            selected: root.preferencesModel.previewOnLeft
                            onClicked: root.preferencesModel.previewOnLeft = true
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
                AppSwitch {
                    objectName: "preferencesBlurMaterialsSwitch"
                    text: qsTrId("dialog.preferences.blur_materials")
                    checked: root.preferencesModel.blurMaterialsEnabled
                    onToggled: root.preferencesModel.blurMaterialsEnabled = checked
                }
            }

            // ---- 背景 ----
            ColumnLayout {
                objectName: "preferencesBackgroundPage"
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                spacing: Theme.settingsRowSpacing

                AppSwitch {
                    text: qsTrId("qml.enable_application_background")
                    checked: root.appBackground.enabled
                    onToggled: root.appBackground.enabled = checked
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        font.pixelSize: Theme.uiFontSize
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
                    font.pixelSize: Theme.uiFontSize
                    Layout.fillWidth: true
                    visible: root.appBackground.errorMessage.length > 0
                    text: root.appBackground.errorMessage
                    color: Theme.colors.syntax.error
                    font.family: Theme.uiFont
                    wrapMode: Text.WordWrap
                }
                LabeledSlider {
                    label: qsTrId("qml.image_opacity")
                    from: 0.1; to: 0.8; stepSize: 0.01
                    value: root.appBackground.opacity
                    readout: Math.round(root.appBackground.opacity * 100) + "%"
                    onMoved: function(value) { root.appBackground.opacity = value }
                }
                LabeledSlider {
                    label: qsTrId("qml.background_mask_opacity")
                    from: 0; to: 1; stepSize: 0.01
                    value: root.appBackground.panelAlpha / 255.0
                    readout: Math.round(root.appBackground.panelAlpha / 255.0 * 100) + "%"
                    onMoved: function(value) { root.appBackground.panelAlpha = Math.round(value * 255) }
                }
                LabeledSlider {
                    label: qsTrId("qml.blur_radius")
                    from: 0; to: 64; stepSize: 1
                    value: root.appBackground.blur
                    readout: Math.round(root.appBackground.blur)
                    onMoved: function(value) { root.appBackground.blur = Math.round(value) }
                }
                LabeledCombo {
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

            // ---- 编辑器 ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                spacing: Theme.settingsRowSpacing

                LabeledSlider {
                    objectName: "preferencesFontSizeSlider"
                    label: qsTrId("dialog.preferences.editor_font_size")
                    from: root.preferencesModel.editorFontSizeMinimum
                    to: root.preferencesModel.editorFontSizeMaximum
                    value: root.preferencesModel.editorFontSize
                    readout: root.preferencesModel.editorFontSize + " pt"
                    onMoved: function(v) { root.preferencesModel.editorFontSize = Math.round(v) }
                }
                LabeledCombo {
                    objectName: "preferencesLineSpacingCombo"
                    label: qsTrId("dialog.preferences.editor_line_spacing")
                    options: root.preferencesModel.lineSpacingOptions
                    currentValue: root.preferencesModel.editorLineSpacing
                    onPicked: function(value) { root.preferencesModel.editorLineSpacing = value }
                }
                LabeledCombo {
                    objectName: "preferencesInputHandlingCombo"
                    label: qsTrId("preferences.input_handling")
                    options: [
                        { value: 0, label: qsTrId("preferences.correct_full_width_only") },
                        { value: 1, label: qsTrId("preferences.block_input_methods_and_correct_full_width") },
                        { value: 2, label: qsTrId("preferences.leave_input_unchanged") }
                    ]
                    currentValue: root.preferencesModel.editorInputHandlingMode
                    onPicked: function(value) { root.preferencesModel.editorInputHandlingMode = value }
                }
                AppSwitch {
                    objectName: "preferencesAutoWrapSwitch"
                    text: qsTrId("preferences.editor_auto_wrap")
                    checked: root.preferencesModel.editorAutoWrap
                    onToggled: root.preferencesModel.editorAutoWrap = checked
                }
                AppSwitch {
                    objectName: "preferencesAutoCompletionSwitch"
                    text: qsTrId("preferences.auto_completion")
                    checked: root.preferencesModel.editorAutoCompletion
                    onToggled: root.preferencesModel.editorAutoCompletion = checked
                }
                AppSwitch {
                    objectName: "preferencesScrollPastEndSwitch"
                    text: qsTrId("preferences.editor_scroll_past_end")
                    checked: root.preferencesModel.editorScrollPastEnd
                    onToggled: root.preferencesModel.editorScrollPastEnd = checked
                }
                AppSwitch {
                    objectName: "preferencesSelectionBeatDisplaySwitch"
                    text: qsTrId("preferences.editor_selection_beat_display")
                    checked: root.preferencesModel.editorSelectionBeatDisplay
                    onToggled: root.preferencesModel.editorSelectionBeatDisplay = checked
                }
            }

            // ---- 性能 ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                spacing: Theme.settingsRowSpacing

                LabeledCombo {
                    objectName: "preferencesVideoDecodeCombo"
                    label: qsTrId("qml.video_decoding")
                    options: [{ value: false, label: qsTrId("qml.hardware_decoding") }, { value: true, label: qsTrId("qml.software_decoding") }]
                    currentValue: root.preferencesModel.videoDecodePrefersSoftware
                    onPicked: function(value) { root.preferencesModel.videoDecodePrefersSoftware = value }
                }
                LabeledCombo {
                    objectName: "preferencesCanvasFrameRateCombo"
                    label: qsTrId("qml.canvas_frame_rate")
                    options: root.preferencesModel.canvasFrameRateOptions
                    currentValue: root.preferencesModel.canvasFrameRateMode
                    onPicked: function(value) { root.preferencesModel.canvasFrameRateMode = value }
                }
                LabeledCombo {
                    objectName: "preferencesPvFrameRateCombo"
                    label: qsTrId("qml.pv_frame_rate")
                    options: root.preferencesModel.appFrameRateOptions
                    currentValue: root.preferencesModel.stageMediaFrameRateMode
                    onPicked: function(value) { root.preferencesModel.stageMediaFrameRateMode = value }
                }
                LabeledCombo {
                    objectName: "preferencesTimelineFrameRateCombo"
                    label: qsTrId("qml.timeline_frame_rate")
                    options: root.preferencesModel.appFrameRateOptions
                    currentValue: root.preferencesModel.timelineFrameRateMode
                    onPicked: function(value) { root.preferencesModel.timelineFrameRateMode = value }
                }
            }

            // ---- 快捷键 ----
            // Clicking a row arms capture: the next key press carrying at least one
            // non-modifier becomes that command's binding. Escape cancels the
            // capture rather than closing 偏好设置, so an accidental arm cannot lose
            // the row being edited.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.settingsRowSpacing

                Text {
                    font.pixelSize: Theme.uiFontSize
                    Layout.fillWidth: true
                    text: root.capturingId.length > 0
                          ? qsTrId("qml.press_a_new_shortcut_esc_cancels")
                          : qsTrId("qml.click_a_row_to_record_a_new_shortcut")
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                }

                ListView {
                    id: shortcutList
                    objectName: "shortcutList"
                    property bool reservesPlainSpace: root.capturingId.length > 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    focus: true
                    model: root.shortcutRows
                    ScrollBar.vertical: AppScrollBar {}

                    Keys.onPressed: function(event) {
                        if (root.capturingId.length === 0)
                            return
                        event.accepted = true
                        if (event.key === Qt.Key_Escape) {
                            root.capturingId = ""
                            return
                        }
                        if (event.key === Qt.Key_Control || event.key === Qt.Key_Shift
                                || event.key === Qt.Key_Alt || event.key === Qt.Key_Meta)
                            return
                        const text = root.shortcuts.shortcutTextForKeyEvent(event.key, event.modifiers)
                        if (text.length === 0)
                            return
                        root.shortcuts.setShortcutText(root.capturingId, text)
                        root.capturingId = ""
                        root.refreshShortcuts()
                    }

                    // The row is NOT one big button: the reset button sits beside
                    // the clickable area, not inside it, so the highlight stops
                    // where the recording hit area stops.
                    delegate: RowLayout {
                        id: shortcutRow
                        required property var modelData
                        width: ListView.view.width
                        spacing: 8

                        ChromeRow {
                            id: captureArea
                            Layout.fillWidth: true
                            implicitHeight: 34
                            selected: root.capturingId === shortcutRow.modelData.id
                            onClicked: {
                                root.capturingId = shortcutRow.modelData.id
                                shortcutList.forceActiveFocus()
                            }
                            contentItem: RowLayout {
                                spacing: 8
                                Text {
                                    font.pixelSize: Theme.uiFontSize
                                    Layout.fillWidth: true
                                    text: qsTrId(shortcutRow.modelData.labelKey)
                                          || shortcutRow.modelData.labelFallback
                                    elide: Text.ElideRight
                                    color: Theme.colors.text.active
                                    font.family: Theme.uiFont
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    font.pixelSize: Theme.uiFontSize
                                    Layout.preferredWidth: 170
                                    text: root.capturingId === shortcutRow.modelData.id
                                          ? qsTrId("qml.recording")
                                          : shortcutRow.modelData.shortcutText
                                    color: shortcutRow.modelData.isDefault
                                           ? Theme.colors.text.secondary
                                           : Theme.colors.accent.primary
                                    font.family: Theme.uiFont
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        AppButton {
                            text: qsTrId("qml.restore_defaults")
                            enabled: !shortcutRow.modelData.isDefault
                            onClicked: {
                                root.shortcuts.resetShortcut(shortcutRow.modelData.id)
                                root.refreshShortcuts()
                            }
                        }
                    }
                }

                AppButton {
                    objectName: "preferencesResetShortcutsButton"
                    Layout.alignment: Qt.AlignLeft
                    text: qsTrId("qml.restore_all_defaults")
                    onClicked: {
                        root.shortcuts.resetAllShortcuts()
                        root.refreshShortcuts()
                    }
                }
            }

            // ---- 更新 ----
            // A manual check always answers — up to date, no package for this
            // platform, or failed — because a check the reader started themselves
            // must not fail silently. An automatic "available" result never lands
            // here: it only sets updateAvailable, surfaced by the status bar badge.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                spacing: Theme.settingsRowSpacing

                AppSwitch {
                    objectName: "preferencesUpdateCheckSwitch"
                    text: qsTrId("dialog.preferences.update.auto_check")
                    checked: root.updateService ? root.updateService.checkEnabled : false
                    onToggled: if (root.updateService) root.updateService.checkEnabled = checked
                }

                LabeledCombo {
                    objectName: "preferencesUpdateChannelCombo"
                    label: qsTrId("dialog.preferences.update.channel")
                    options: [
                        { value: "stable", label: qsTrId("dialog.preferences.update.channel.stable") },
                        { value: "beta", label: qsTrId("dialog.preferences.update.channel.beta") }
                    ]
                    currentValue: root.updateService ? root.updateService.effectiveChannel() : "stable"
                    onPicked: function(value) { if (root.updateService) root.updateService.channelToken = value }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.updateService && root.updateService.lastCheckText.length > 0
                    text: qsTrId("dialog.preferences.update.last_check").arg(
                        root.updateService ? root.updateService.lastCheckText : "")
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.secondaryFontSize
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    spacing: 8

                    AppButton {
                        objectName: "preferencesUpdateCheckNowButton"
                        text: qsTrId("dialog.preferences.update.check_now")
                        enabled: root.updateService && !root.updateService.checkInFlight
                        onClicked: if (root.updateService) root.updateService.checkNow(true)
                    }

                    Text {
                        id: updateManualResult
                        property string outcome: ""
                        text: {
                            if (outcome === "up-to-date") return qsTrId("dialog.preferences.update.up_to_date")
                            if (outcome === "no-package") return qsTrId("dialog.preferences.update.no_package")
                            if (outcome === "" || outcome === "available") return ""
                            return qsTrId("dialog.preferences.update.failed")
                        }
                        objectName: "preferencesUpdateManualResultText"
                        Layout.fillWidth: true
                        color: Theme.colors.text.secondary
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.secondaryFontSize
                        wrapMode: Text.WordWrap
                    }
                }

                Connections {
                    target: root.updateService
                    function onManualCheckFinished(outcome, detail) {
                        updateManualResult.outcome = outcome
                        if (outcome === "available") {
                            root.updateRequested()
                            return
                        }
                    }
                }
            }
        }
    }

}
