import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root
    required property var preferencesModel
    property bool expanded: false
    required property Flickable scrollViewport
    signal ensureVisibleRequested(Item item, string alignment)
    signal scrollRequested(real distance)
    implicitHeight: heading.height + (root.expanded ? contents.height : 0)
    clip: true

    onExpandedChanged: {
        if (!expanded) {
            root.releaseInputFocus()
            root.preferencesModel.finishCompletionEditing()
        }
    }
    onVisibleChanged: {
        if (!visible) {
            root.releaseInputFocus()
            root.expanded = false
        }
    }

    function ensureContentLayout() {
        durations.ensureContentLayout()
        divisions.ensureContentLayout()
        groups.ensurePolished()
        contentColumn.ensurePolished()
    }
    function releaseInputFocusOutside(origin, position) {
        durations.releaseInputFocusOutside(origin, position)
        divisions.releaseInputFocusOutside(origin, position)
    }
    function releaseInputFocus() {
        durations.releaseInputFocus()
        divisions.releaseInputFocus()
    }
    function openGroup(group) {
        root.expanded = true
        Qt.callLater(function() {
            if (root.visible && root.expanded) {
                root.ensureContentLayout()
                root.ensureVisibleRequested(group === "division" ? divisions : durations, "top")
            }
        })
    }

    ChromeRow {
        id: heading
        objectName: "completionCustomizeToggle"
        width: parent.width
        implicitHeight: Theme.controlMinHeight + 10
        leftPadding: 0
        rightPadding: 6
        text: qsTrId("preferences.completion.customize")
        Accessible.checkable: true
        Accessible.checked: root.expanded
        onClicked: root.expanded = !root.expanded
        contentItem: RowLayout {
            spacing: Theme.settingsRowSpacing
            Text {
                Layout.fillWidth: true
                text: heading.text
                color: heading.hovered || heading.visualFocus
                    ? Theme.colors.text.active : Theme.colors.text.secondary
                font: heading.labelFont
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            ControlsImpl.IconImage {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: Qt.resolvedUrl(root.expanded ? "icons/chevron-down.svg" : "icons/chevron-right.svg")
                sourceSize: Theme.iconSourceSize(16, 16)
                color: Theme.colors.text.secondary
            }
        }
    }

    TapHandler {
        parent: root
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        gesturePolicy: TapHandler.DragThreshold
        onPressedChanged: {
            if (pressed) root.releaseInputFocusOutside(root, point.position)
        }
    }

    Rectangle {
        id: contents
        y: heading.height
        width: parent.width
        height: contentColumn.implicitHeight + 24
        radius: Theme.controlRadius
        color: Theme.surfaceColor(Theme.colors.background.surface)
        border.width: Theme.controlBorderWidth
        border.color: Theme.colors.border.normal
        visible: root.expanded
        enabled: root.expanded
        ColumnLayout {
            id: contentColumn
            x: 12
            y: 12
            width: Math.max(0, parent.width - 24)
            spacing: 12

            RowLayout {
                id: groups
                Layout.fillWidth: true
                spacing: 20
                CompletionCandidateList {
                    id: durations
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    candidateModel: root.preferencesModel.completionDurations
                    title: qsTrId("preferences.completion.duration")
                    inputHint: qsTrId("preferences.completion.duration_hint")
                    scrollViewport: root.scrollViewport
                    onEnsureVisibleRequested: function(item) { root.ensureVisibleRequested(item, "contain") }
                    onScrollRequested: function(distance) { root.scrollRequested(distance) }
                }
                CompletionCandidateList {
                    id: divisions
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    candidateModel: root.preferencesModel.completionDivisions
                    title: qsTrId("preferences.completion.division")
                    inputHint: qsTrId("preferences.completion.division_hint")
                    scrollViewport: root.scrollViewport
                    onEnsureVisibleRequested: function(item) { root.ensureVisibleRequested(item, "contain") }
                    onScrollRequested: function(distance) { root.scrollRequested(distance) }
                }
            }

            AppButton {
                id: resetButton
                Layout.alignment: Qt.AlignRight
                text: qsTrId("preferences.completion.reset")
                Accessible.description: qsTrId("preferences.completion.reset_hint")
                onClicked: resetDialog.open()
                Tooltip {
                    visible: resetButton.hovered
                    text: qsTrId("preferences.completion.reset_hint")
                }
            }
        }
    }

    ChoiceDialog {
        id: resetDialog
        title: qsTrId("preferences.completion.reset_question")
        dismissChoiceId: "cancel"
        choices: [
            { id: "cancel", label: qsTrId("action.cancel"), role: "reject" },
            { id: "reset", label: qsTrId("preferences.completion.reset"), role: "accept" }
        ]
        onChosen: function(choiceId) {
            if (choiceId === "reset") root.preferencesModel.resetCompletionCandidates()
        }
    }
}
