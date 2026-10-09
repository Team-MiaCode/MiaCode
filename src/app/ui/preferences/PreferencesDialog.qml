import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// 偏好设置. Every control writes straight through to the model, which persists
// on each change — there is no OK/Apply, matching the Widgets dialog it
// replaces. Language is applied live; theme and platform decoder restart
// requirements are reported by the model.
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
    property Component platformOptions: null

    signal updateRequested()

    title: qsTrId("dialog.preferences.title")
    preferredWidth: 700
    preferredHeight: Theme.dialogHeight
    fillBody: true
    footer: DialogFooter {
        cancelText: qsTrId("action.close")
        onRejected: root.reject()
    }

    property int activePage: 0
    // Id of the command whose binding is being recorded; "" when idle.
    property string capturingId: ""
    property var shortcutRows: []

    onActivePageChanged: root.capturingId = ""
    onAboutToShow: {
        root.capturingId = ""
        root.refreshShortcuts()
    }
    Connections {
        target: root.preferencesModel
        function onInterfaceChanged() {
            if (root.visible && root.capturingId.length === 0) root.refreshShortcuts()
        }
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

    body: ColumnLayout {
        spacing: 10

        Row {
            spacing: 4
            Repeater {
                model: [qsTrId("qml.interface"), qsTrId("dialog.preferences.background_group"), qsTrId("dialog.preferences.editor_group"), qsTrId("dialog.preferences.performance_group"), qsTrId("dialog.preferences.shortcuts_group"), qsTrId("dialog.preferences.updates_group")]
                delegate: AppTab {
                    required property int index
                    required property string modelData
                    panelTab: true
                    text: modelData
                    active: root.activePage === index
                    onClicked: root.activePage = index
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.colors.border.normal
        }

        // ---- 界面 ----
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.activePage === 0
            spacing: 10

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
            LabeledCombo {
                objectName: "preferencesPreviewSideCombo"
                label: qsTrId("dialog.preferences.preview_side")
                options: [{ value: false, label: qsTrId("qml.right") }, { value: true, label: qsTrId("qml.left") }]
                currentValue: root.preferencesModel.previewOnLeft
                onPicked: function(value) { root.preferencesModel.previewOnLeft = value }
            }
            Text {
                objectName: "preferencesRestartHint"
                Layout.fillWidth: true
                visible: root.preferencesModel.restartRequired
                text: qsTrId("qml.theme_changes_take_effect_after_restarting")
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                wrapMode: Text.WordWrap
            }
            Loader {
                Layout.fillWidth: true
                sourceComponent: root.platformOptions
                visible: sourceComponent !== null
            }
        }

        // ---- 背景 ----
        AppBackgroundPage {
            Layout.fillWidth: true
            visible: root.activePage === 1
            appBackground: root.appBackground
        }

        // ---- 编辑器 ----
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.activePage === 2
            spacing: 10

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
            visible: root.activePage === 3
            spacing: 10

            LabeledCombo {
                objectName: "preferencesVideoDecodeCombo"
                label: qsTrId("qml.video_decoding")
                options: [{ value: false, label: qsTrId("qml.hardware_decoding") }, { value: true, label: qsTrId("qml.software_decoding") }]
                currentValue: root.preferencesModel.videoDecodePrefersSoftware
                onPicked: function(value) { root.preferencesModel.videoDecodePrefersSoftware = value }
            }
            Text {
                objectName: "preferencesDecoderRestartHint"
                Layout.fillWidth: true
                visible: root.preferencesModel.decoderRestartRequired
                text: qsTrId("preferences.decoder_restart_required")
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                wrapMode: Text.WordWrap
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
            visible: root.activePage === 4
            spacing: 8

            Text {
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

                Keys.onShortcutOverride: event => {
                    if (root.capturingId.length > 0) event.accepted = true
                }

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
                                Layout.fillWidth: true
                                text: qsTrId(shortcutRow.modelData.labelKey)
                                      || shortcutRow.modelData.labelFallback
                                elide: Text.ElideRight
                                color: Theme.colors.text.active
                                font.family: Theme.uiFont
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
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
            visible: root.activePage === 5
            spacing: 10

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
                    if (outcome === "available") {
                        updateManualResult.text = ""
                        root.updateRequested()
                        return
                    }
                    if (outcome === "up-to-date")
                        updateManualResult.text = qsTrId("dialog.preferences.update.up_to_date")
                    else if (outcome === "no-package")
                        updateManualResult.text = qsTrId("dialog.preferences.update.no_package")
                    else
                        updateManualResult.text = qsTrId("dialog.preferences.update.failed")
                }
            }
        }

        // The shortcut page owns the slack itself; this only pads the pages
        // whose controls are a short stack at the top.
        Item {
            Layout.fillHeight: true
            visible: root.activePage !== 4
        }
    }

}
