import QtQuick
import QtQuick.Controls
import MiaCode.UI

// Shared text operations; the editor retains its native keyboard handling.
AppMenu {
    id: root

    required property Item editor
    readonly property bool writable: editor.enabled && !editor.readOnly
    readonly property bool sensitive: editor instanceof TextInput
        && editor.echoMode !== TextInput.Normal
    readonly property bool hasSelection: editor.selectionStart !== editor.selectionEnd
    property bool previousPersistentSelection: false

    Connections {
        target: root
        function onAboutToShow() {
            root.previousPersistentSelection = root.editor.persistentSelection
            root.editor.persistentSelection = true
            root.editor.forceActiveFocus(Qt.PopupFocusReason)
        }
        function onClosed() {
            root.editor.persistentSelection = root.previousPersistentSelection
        }
    }

    function perform(operation) {
        editor.forceActiveFocus(Qt.PopupFocusReason)
        switch (operation) {
        case "undo": editor.undo(); break
        case "redo": editor.redo(); break
        case "cut": editor.cut(); break
        case "copy": editor.copy(); break
        case "paste": editor.paste(); break
        case "selectAll": editor.selectAll(); break
        }
    }

    AppMenuItem {
        text: qsTrId("action.undo")
        visible: root.writable
        height: visible ? implicitHeight : 0
        enabled: root.writable && root.editor.canUndo
        onTriggered: root.perform("undo")
    }
    AppMenuItem {
        text: qsTrId("action.redo")
        visible: root.writable
        height: visible ? implicitHeight : 0
        enabled: root.writable && root.editor.canRedo
        onTriggered: root.perform("redo")
    }
    AppMenuSeparator {
        visible: root.writable
        height: visible ? implicitHeight : 0
    }
    AppMenuItem {
        text: qsTrId("action.cut")
        visible: root.writable && !root.sensitive
        height: visible ? implicitHeight : 0
        enabled: root.writable && root.hasSelection && !root.sensitive
        onTriggered: root.perform("cut")
    }
    AppMenuItem {
        text: qsTrId("action.copy")
        visible: !root.sensitive
        height: visible ? implicitHeight : 0
        enabled: root.editor.enabled && root.hasSelection && !root.sensitive
        onTriggered: root.perform("copy")
    }
    AppMenuItem {
        text: qsTrId("action.paste")
        visible: root.writable
        height: visible ? implicitHeight : 0
        enabled: root.writable && root.editor.canPaste
        onTriggered: root.perform("paste")
    }
    AppMenuSeparator {
        visible: root.writable || !root.sensitive
        height: visible ? implicitHeight : 0
    }
    AppMenuItem {
        text: qsTrId("net.select_all")
        enabled: root.editor.enabled && root.editor.length > 0
        onTriggered: root.perform("selectAll")
    }
}
