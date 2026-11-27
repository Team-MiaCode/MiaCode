import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

Rectangle {
    id: root

    required property var editor
    property bool open: false
    property string query: ""
    property string replacement: ""
    property bool caseSensitive: false
    property bool wholeWord: false

    clip: true
    height: open ? implicitHeight : 0
    implicitHeight: layout.implicitHeight + 12
    visible: height > 0.5
    color: Theme.overlayColor(Theme.colors.background.panel, Theme.popupOpacity)

    Behavior on height {
        enabled: Theme.animationsEnabled
        NumberAnimation { duration: Theme.panelSlideDuration; easing.type: Easing.OutCubic }
    }

    function configureSearch() {
        editor.configureSearch(query, replacement, caseSensitive, wholeWord)
    }
    function find(backwards) {
        configureSearch()
        if (backwards) editor.findPrevious()
        else editor.findNext()
    }
    function replaceOne() {
        configureSearch()
        editor.replaceAndFind()
    }
    function replaceEverything() {
        configureSearch()
        editor.replaceAll()
    }
    function show() {
        open = true
        Qt.callLater(() => queryField.forceActiveFocus())
    }
    function dismiss() {
        open = false
        editor.forceActiveFocus()
    }

    Keys.onEscapePressed: event => {
        dismiss()
        event.accepted = true
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            AppTextField {
                id: queryField
                Layout.fillWidth: true
                Layout.minimumWidth: 110
                placeholderText: qsTrId("metadata.find")
                text: root.query
                onTextEdited: root.query = text
                Keys.onReturnPressed: event => root.find((event.modifiers & Qt.ShiftModifier) !== 0)
            }
            AppTextField {
                id: replacementField
                Layout.fillWidth: true
                Layout.minimumWidth: 110
                placeholderText: qsTrId("metadata.replace")
                text: root.replacement
                onTextEdited: root.replacement = text
                Keys.onReturnPressed: root.replaceOne()
            }
            IconButton {
                compact: true
                iconSource: Qt.resolvedUrl("icons/close.svg")
                tooltip: qsTrId("action.close")
                onClicked: root.dismiss()
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6

            AppSwitch {
                Layout.fillWidth: false
                text: qsTrId("qml.match_case")
                checked: root.caseSensitive
                onToggled: root.caseSensitive = checked
            }
            AppSwitch {
                Layout.fillWidth: false
                text: qsTrId("qml.whole_words")
                checked: root.wholeWord
                onToggled: root.wholeWord = checked
            }
            AppButton {
                minimumButtonWidth: 72
                text: qsTrId("qml.previous")
                onClicked: root.find(true)
            }
            AppButton {
                minimumButtonWidth: 72
                text: qsTrId("qml.next")
                onClicked: root.find(false)
            }
            AppButton {
                minimumButtonWidth: 72
                text: qsTrId("metadata.replace")
                emphasized: true
                onClicked: root.replaceOne()
            }
            AppButton {
                minimumButtonWidth: 72
                text: qsTrId("metadata.replace_all")
                emphasized: true
                onClicked: root.replaceEverything()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1 / (Screen.devicePixelRatio * Theme.uiScale)
        color: Theme.separatorColor
    }
}
