import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

AppDialog {
    id: root

    required property var preferences
    required property var preferencesModel
    property int step: 0
    property bool completed: false
    property string saveError: ""
    readonly property var titles: [
        qsTrId("setup.welcome"),
        qsTrId("dialog.preferences.group.appearance"),
        qsTrId("dialog.preferences.interface_layout"),
        qsTrId("dialog.preferences.sidebar_mode"),
        qsTrId("dialog.preferences.editor_group")
    ]

    signal finished()

    title: titles[step]
    preferredWidth: 660
    preferredHeight: 360
    fillBody: root.step === 0
    closePolicy: Popup.NoAutoClose

    function advance() {
        if (step < titles.length - 1) {
            step += 1
            return
        }
        finishSetup()
    }

    function finishSetup() {
        if (!preferences.completeSetup()) {
            saveError = qsTrId("setup.save_failed")
            return
        }
        completed = true
        close()
    }

    onClosed: if (completed) finished()

    body: ColumnLayout {
        spacing: Theme.dialogPadding

        ColumnLayout {
            visible: root.step === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.dialogPadding

            Item { Layout.fillHeight: true }

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 96
                Layout.preferredHeight: 96
                source: "qrc:/icons/app.png"
                sourceSize: Qt.size(192, 192)
                fillMode: Image.PreserveAspectFit
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.preferences.preferenceMigrationStatus === "imported"
                    ? qsTrId("setup.imported")
                    : root.preferences.preferenceMigrationStatus === "pending"
                        ? qsTrId("setup.import_pending") : qsTrId("setup.configure")
                color: Theme.colors.text.primary
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }
        }

        LabeledCombo {
            objectName: "setupThemeMode"
            visible: root.step === 1
            label: qsTrId("dialog.preferences.theme.mode")
            options: root.preferencesModel.themeModeOptions
            currentValue: root.preferencesModel.themeModeToken
            onPicked: value => root.preferencesModel.themeModeToken = value
        }
        LabeledCombo {
            objectName: "setupLightTheme"
            visible: root.step === 1 && root.preferencesModel.themeModeToken !== "dark"
            label: qsTrId("dialog.preferences.theme.light_palette")
            options: root.preferencesModel.themePaletteOptions
            currentValue: root.preferencesModel.lightThemeToken
            onPicked: value => root.preferencesModel.lightThemeToken = value
        }
        LabeledCombo {
            objectName: "setupDarkTheme"
            visible: root.step === 1 && root.preferencesModel.themeModeToken !== "light"
            label: qsTrId("dialog.preferences.theme.dark_palette")
            options: root.preferencesModel.themePaletteOptions
            currentValue: root.preferencesModel.darkThemeToken
            onPicked: value => root.preferencesModel.darkThemeToken = value
        }
        LabeledCombo {
            objectName: "setupUiScale"
            visible: root.step === 1
            label: qsTrId("dialog.preferences.ui_scale")
            options: root.preferencesModel.uiScaleOptions
            currentValue: root.preferencesModel.uiScalePercent
            onPicked: value => root.preferencesModel.uiScalePercent = value
        }
        GridLayout {
            objectName: "setupLayoutCards"
            visible: root.step === 2
            Layout.alignment: Qt.AlignHCenter
            columns: 2
            columnSpacing: 12

            PreviewPositionCard {
                objectName: "setupPreviewRightCard"
                Layout.preferredWidth: Math.min(200,
                    (root.width - 2 * Theme.dialogPadding - 12) / 2)
                previewOnLeft: false
                selected: !root.preferencesModel.previewOnLeft
                onClicked: root.preferencesModel.previewOnLeft = false
            }
            PreviewPositionCard {
                objectName: "setupPreviewLeftCard"
                Layout.preferredWidth: Math.min(200,
                    (root.width - 2 * Theme.dialogPadding - 12) / 2)
                previewOnLeft: true
                selected: root.preferencesModel.previewOnLeft
                onClicked: root.preferencesModel.previewOnLeft = true
            }
        }
        GridLayout {
            objectName: "setupSidebarCards"
            visible: root.step === 3
            Layout.alignment: Qt.AlignHCenter
            columns: 2
            columnSpacing: 12

            SidebarModeCard {
                objectName: "setupSidebarStandardCard"
                Layout.preferredWidth: Math.min(200,
                    (root.width - 2 * Theme.dialogPadding - 12) / 2)
                classic: false
                selected: !root.preferences.efficientSidebarEnabled
                onClicked: root.preferences.efficientSidebarEnabled = false
            }
            SidebarModeCard {
                objectName: "setupSidebarClassicCard"
                Layout.preferredWidth: Math.min(200,
                    (root.width - 2 * Theme.dialogPadding - 12) / 2)
                classic: true
                selected: root.preferences.efficientSidebarEnabled
                onClicked: root.preferences.efficientSidebarEnabled = true
            }
        }
        Text {
            visible: root.step === 3
            Layout.fillWidth: true
            text: qsTrId("setup.navigation_hint")
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.family: Theme.uiFont
            font.pixelSize: Theme.secondaryFontSize
            color: Theme.colors.text.secondary
        }
        LabeledSlider {
            objectName: "setupEditorFontSize"
            visible: root.step === 4
            label: qsTrId("dialog.preferences.editor_font_size")
            from: root.preferencesModel.editorFontSizeMinimum
            to: root.preferencesModel.editorFontSizeMaximum
            value: root.preferencesModel.editorFontSize
            readout: root.preferencesModel.editorFontSize + " pt"
            onMoved: value => root.preferencesModel.editorFontSize = Math.round(value)
        }
        LabeledCombo {
            objectName: "setupEditorLineSpacing"
            visible: root.step === 4
            label: qsTrId("dialog.preferences.editor_line_spacing")
            options: root.preferencesModel.lineSpacingOptions
            currentValue: root.preferencesModel.editorLineSpacing
            onPicked: value => root.preferencesModel.editorLineSpacing = value
        }
        LabeledCombo {
            objectName: "setupInputHandling"
            visible: root.step === 4
            label: qsTrId("preferences.input_handling")
            options: root.preferencesModel.inputHandlingOptions
            currentValue: root.preferencesModel.editorInputHandlingMode
            onPicked: value => root.preferencesModel.editorInputHandlingMode = value
        }
        AppSwitch {
            objectName: "setupAutoCompletion"
            visible: root.step === 4
            text: qsTrId("preferences.auto_completion")
            checked: root.preferencesModel.editorAutoCompletion
            onToggled: root.preferencesModel.editorAutoCompletion = checked
        }
        Text {
            objectName: "setupCompletionHint"
            visible: root.step === 4
            Layout.fillWidth: true
            text: qsTrId("setup.completion_hint")
            wrapMode: Text.WordWrap
            font.family: Theme.uiFont
            font.pixelSize: Theme.secondaryFontSize
            color: Theme.colors.text.secondary
        }
        Text {
            Layout.fillWidth: true
            visible: root.saveError.length > 0
            text: root.saveError
            color: Theme.colors.text.primary
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            wrapMode: Text.WordWrap
        }
    }

    footer: Item {
        implicitHeight: actions.implicitHeight + 2 * Theme.dialogPadding
        RowLayout {
            id: actions
            anchors.fill: parent
            anchors.margins: Theme.dialogPadding
            spacing: Theme.panelPadding

            Text {
                text: qsTrId("guide.step_counter").arg(root.step + 1).arg(root.titles.length)
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.secondaryFontSize
            }
            Item { Layout.fillWidth: true }
            AppButton {
                objectName: "setupPrevious"
                visible: root.step > 0
                text: qsTrId("guide.previous")
                onClicked: root.step -= 1
            }
            AppButton {
                objectName: "setupSkip"
                visible: root.step === 0
                text: qsTrId("setup.skip")
                onClicked: root.finishSetup()
            }
            AppButton {
                id: nextButton
                objectName: "setupNext"
                text: root.step === root.titles.length - 1
                    ? qsTrId("setup.finish") : qsTrId("guide.next")
                emphasized: true
                onClicked: root.advance()
            }
        }
    }
    onAboutToShow: {
        step = 0
        completed = false
        saveError = ""
    }
    onOpened: nextButton.forceActiveFocus()
}
