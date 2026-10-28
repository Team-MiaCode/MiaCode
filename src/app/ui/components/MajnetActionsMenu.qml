import QtQuick
import MiaCode.UI

AppMenu {
    id: root

    property bool actionsEnabled: true
    property bool uploadEnabled: false

    signal downloadRequested()
    signal uploadRequested()

    AppMenuAction {
        text: qsTrId("net.ui.download_page")
        enabled: root.actionsEnabled
        onTriggered: root.downloadRequested()
    }
    Instantiator {
        model: root.uploadEnabled ? 1 : 0
        delegate: AppMenuAction {
            text: qsTrId("net.ui.upload_page")
            enabled: root.actionsEnabled
            onTriggered: root.uploadRequested()
        }
        onObjectAdded: (index, action) => root.insertAction(index + 1, action)
        onObjectRemoved: (index, action) => root.removeAction(action)
    }
}
