pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

AppDialog {
    id: root
    objectName: "coverBatchExportDialog"
    required property var coverSession
    required property var controller
    readonly property bool running: !!controller && controller.running
    property var selectedDifficulties: []
    property var selectedPresets: []
    title: qsTrId("cover.batch_title")
    preferredWidth: 860
    preferredHeight: 560
    closePolicy: root.running ? Popup.NoAutoClose : Popup.CloseOnEscape

    function presetKey(row) { return row.kind + ":" + row.name }
    function toggleDifficulty(id, checked) {
        const selected = root.selectedDifficulties.filter(value => value !== id)
        if (checked) selected.push(id)
        root.selectedDifficulties = selected
    }
    function togglePreset(row, checked) {
        const key = root.presetKey(row)
        const selected = root.selectedPresets.filter(value => root.presetKey(value) !== key)
        if (checked) selected.push(row)
        root.selectedPresets = selected
    }
    onOpened: {
        if (!root.running && root.selectedPresets.length === 0 && root.controller) {
            root.selectedDifficulties = root.coverSession ? [root.coverSession.selectedDifficultyId] : []
            root.selectedPresets = [root.controller.presets[0]]
        }
    }
    body: ColumnLayout {
        spacing: Theme.panelPadding
        Label {
            Layout.fillWidth: true
            text: qsTrId("cover.batch_description")
            wrapMode: Text.WordWrap
            color: Theme.colors.text.secondary
        }
        Label { text: qsTrId("dialog.batch_export.difficulty"); color: Theme.colors.text.heading }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.panelPadding
            Repeater {
                model: root.coverSession ? root.coverSession.difficulties : []
                delegate: AppCheckBox {
                    required property var modelData
                    objectName: "coverBatchDifficulty_" + modelData.id
                    text: modelData.name
                    enabled: !root.running
                    checked: root.selectedDifficulties.indexOf(modelData.id) >= 0
                    onClicked: root.toggleDifficulty(modelData.id, checked)
                }
            }
        }
        Label { text: qsTrId("cover.batch_presets"); color: Theme.colors.text.heading }
        Flow {
            Layout.fillWidth: true
            spacing: Theme.panelPadding
            Repeater {
                model: root.controller ? root.controller.presets : []
                delegate: AppCheckBox {
                    required property var modelData
                    objectName: "coverBatchPreset_" + modelData.kind + "_" + modelData.name
                    text: modelData.label
                    enabled: !root.running
                    checked: root.selectedPresets.some(value => root.presetKey(value) === root.presetKey(modelData))
                    onClicked: root.togglePreset(modelData, checked)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTrId("cover.batch_output_directory"); color: Theme.colors.text.heading }
            Label {
                Layout.fillWidth: true
                text: root.coverSession ? root.coverSession.outputDirectoryDisplay : ""
                wrapMode: Text.WrapAnywhere
                color: Theme.colors.text.secondary
            }
            AppButton {
                objectName: "coverBatchBrowseOutput"
                text: qsTrId("cover.browse")
                enabled: !root.running && !!root.coverSession
                onClicked: root.coverSession.browseOutputDirectory()
            }
        }
        Label {
            Layout.fillWidth: true
            visible: !!root.controller && root.controller.error.length > 0
            text: root.controller ? root.controller.error : ""
            color: Theme.colors.text.primary
            wrapMode: Text.WordWrap
        }
        ProgressBar {
            Layout.fillWidth: true
            visible: !!root.controller && root.controller.total > 0
            from: 0
            to: root.controller ? Math.max(1, root.controller.total) : 1
            value: root.controller ? root.controller.completed : 0
        }
        Label {
            visible: !!root.controller && root.controller.total > 0
            text: root.controller ? qsTrId("cover.batch_progress").arg(root.controller.completed).arg(root.controller.total) : ""
            color: Theme.colors.text.secondary
        }
        Repeater {
            model: root.controller ? root.controller.results : []
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: modelData.label + " — " + qsTrId("cover.batch_status_" + modelData.status)
                    wrapMode: Text.WordWrap
                    color: Theme.colors.text.primary
                }
                Label {
                    Layout.fillWidth: true
                    visible: modelData.path.length > 0 || modelData.error.length > 0
                    text: modelData.error.length > 0 ? modelData.error : modelData.path
                    wrapMode: Text.WrapAnywhere
                    color: Theme.colors.text.secondary
                }
            }
        }
    }
    footer: DialogFooter {
        choices: [
            { id: "start", label: qsTrId("cover.batch_start"), role: "accept",
              enabled: !root.running && root.selectedDifficulties.length > 0 && root.selectedPresets.length > 0 },
            { id: "close", label: root.running ? qsTrId("action.cancel") : qsTrId("action.close"),
              enabled: !root.running || !root.controller.cancelRequested }
        ]
        onChosen: choiceId => {
            if (choiceId === "start") root.controller.start(root.selectedDifficulties, root.selectedPresets)
            else if (root.running) root.controller.cancel()
            else root.close()
        }
    }
}
