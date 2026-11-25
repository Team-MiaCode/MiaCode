pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

AppTextField {
    id: root

    required property var suggestions
    signal suggestionsRequested()
    signal suggestionsDismissed()
    signal searchRequested()

    property bool suggestionsSuppressed: false
    property bool searchSubmitted: false
    property int suggestionIndex: -1
    readonly property bool showSuggestions: visible && enabled && activeFocus && !inputMethodComposing
        && !!Window.window?.active && !suggestionsSuppressed && suggestionRows.length > 0
    readonly property var suggestionRows: text.trim().length > 0 ? suggestions : [
        {label: "id:", value: "id:", hint: qsTrId("net.ui.search_by_id"), prefix: true},
        {label: "hash:", value: "hash:", hint: qsTrId("net.ui.search_by_hash"), prefix: true},
        {label: "tag:", value: "tag:", hint: qsTrId("net.ui.tag"), prefix: true},
        {label: "uploader:", value: "uploader:", hint: qsTrId("net.ui.user_id"), prefix: true}
    ]

    function dismissSuggestions() {
        suggestionTimer.stop()
        suggestionsSuppressed = true
        suggestionIndex = -1
        suggestionsDismissed()
    }

    function refreshSuggestions() {
        // Re-enabling the search controls can restore focus after a query.
        if (searchSubmitted)
            return
        dismissSuggestions()
        suggestionsSuppressed = false
        if (activeFocus && Window.window?.active && text.trim().length > 0 && !inputMethodComposing)
            suggestionTimer.restart()
    }

    function suppressSuggestionsAfterSearch() {
        searchSubmitted = true
        dismissSuggestions()
    }

    function resumeSuggestions() {
        searchSubmitted = false
        refreshSuggestions()
    }

    function focusForSearch() {
        forceActiveFocus()
        resumeSuggestions()
    }

    function pick(row) {
        text = row.value
        cursorPosition = text.length
        if (row.prefix) refreshSuggestions()
        else searchRequested()
    }

    function moveSuggestion(step) {
        const count = suggestionRows.length
        if (!suggestionPopup.active || count === 0)
            return false
        suggestionIndex = suggestionIndex < 0 ? (step < 0 ? count - 1 : 0)
            : (suggestionIndex + step + count) % count
        return true
    }

    function suggestionLabel(kind) {
        switch (kind) {
        case "title": return qsTrId("net.ui.song_title")
        case "artist": return qsTrId("net.ui.artist")
        case "designer": return qsTrId("net.designer")
        case "uploader": return qsTrId("net.ui.user_id")
        case "tag": return qsTrId("net.ui.tag")
        default: return ""
        }
    }

    outlined: true
    placeholderText: qsTrId("net.ui.search_placeholder")
    Accessible.name: placeholderText
    leftPadding: searchButton.width + 2
    rightPadding: clearButton.visible ? clearButton.width : 10
    maximumLength: 512
    onTextEdited: resumeSuggestions()
    onInputMethodComposingChanged: refreshSuggestions()
    onActiveFocusChanged: {
        if (activeFocus) refreshSuggestions()
        else dismissSuggestions()
    }
    onSuggestionsChanged: suggestionIndex = -1
    onAccepted: {
        if (suggestionPopup.active && suggestionIndex >= 0 && suggestionIndex < suggestionRows.length)
            pick(suggestionRows[suggestionIndex])
        else searchRequested()
    }
    Keys.onDownPressed: event => event.accepted = !root.inputMethodComposing && root.moveSuggestion(1)
    Keys.onUpPressed: event => event.accepted = !root.inputMethodComposing && root.moveSuggestion(-1)
    Keys.onEscapePressed: event => {
        event.accepted = suggestionPopup.active && !root.inputMethodComposing
        if (event.accepted) root.dismissSuggestions()
    }

    TapHandler {
        onTapped: point => {
            const x = point.position.x
            if (root.suggestionsSuppressed && x >= searchButton.width && x < root.width - root.rightPadding)
                root.resumeSuggestions()
        }
    }

    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (!root.Window.window?.active) root.dismissSuggestions()
        }
    }

    Timer {
        id: suggestionTimer
        interval: 200
        onTriggered: root.suggestionsRequested()
    }

    IconButton {
        id: searchButton
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        compact: true
        focusPolicy: Qt.NoFocus
        iconSource: Qt.resolvedUrl("icons/search.svg")
        tooltip: qsTrId("net.ui.query")
        onClicked: {
            if (root.text.trim().length > 0) root.searchRequested()
            else root.focusForSearch()
        }
    }

    IconButton {
        id: clearButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        compact: true
        visible: root.text.length > 0
        focusPolicy: Qt.NoFocus
        iconSource: Qt.resolvedUrl("icons/close.svg")
        tooltip: qsTrId("net.ui.clear_search")
        onClicked: {
            root.clear()
            root.focusForSearch()
        }
    }

    AppDropdownPanel {
        id: suggestionPopup
        parent: root
        y: root.height
        width: root.width
        focus: false
        closePolicy: Popup.CloseOnPressOutsideParent
        visible: root.showSuggestions
        onAboutToHide: if (root.showSuggestions) root.dismissSuggestions()

        contentItem: Column {
            Accessible.name: qsTrId("net.ui.search_suggestions")
            Repeater {
                model: root.suggestionRows
                delegate: ChromeRow {
                    id: option
                    required property int index
                    required property var modelData
                    readonly property string hint: modelData.hint || root.suggestionLabel(modelData.kind)
                    width: parent.width
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: false
                    stateColors: Theme.colors.popupState
                    highlightFillsBounds: true
                    highlighted: index === root.suggestionIndex
                    text: modelData.label
                    Accessible.name: text + ", " + hint
                    onClicked: root.pick(modelData)

                    HoverHandler {
                        onHoveredChanged: if (hovered) root.suggestionIndex = option.index
                        onPointChanged: if (hovered) root.suggestionIndex = option.index
                    }

                    contentItem: Row {
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.alignWhenCentered: false
                            width: Math.max(0, parent.width - hintLabel.implicitWidth - parent.spacing)
                            text: option.text
                            color: Theme.colors.text.primary
                            font: option.labelFont
                            elide: Text.ElideRight
                        }
                        Text {
                            id: hintLabel
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.alignWhenCentered: false
                            text: option.hint
                            color: Theme.colors.text.secondary
                            font: option.labelFont
                        }
                    }
                }
            }
        }
    }
}
