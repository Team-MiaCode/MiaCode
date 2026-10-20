import QtQuick
import MiaCode.UI

AppMenu {
    id: root

    property bool actionsEnabled: true

    signal downloadRequested()
    signal uploadRequested()

    AppMenuAction {
        text: qsTrId("net.ui.download_page")
        enabled: root.actionsEnabled
        onTriggered: root.downloadRequested()
    }
    AppMenuAction {
        text: qsTrId("net.ui.upload_page")
        enabled: root.actionsEnabled
        onTriggered: root.uploadRequested()
    }
}
