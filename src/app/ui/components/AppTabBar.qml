import QtQuick
import QtQuick.Controls
import MiaCode.UI

TabBar {
    id: root

    property var tabs: []
    property string selectedId: ""
    property string buttonObjectNamePrefix: ""
    signal tabSelected(string tabId)

    implicitWidth: contentWidth
    implicitHeight: Theme.controlMinHeight
    padding: 0
    spacing: 4
    background: null

    function indexForId(tabId) {
        return tabs.findIndex(tab => tab.id === tabId)
    }

    function selectTab(index) {
        const tab = tabs[index]
        setCurrentIndex(index)
        itemAt(index).forceActiveFocus(Qt.TabFocusReason)
        tabSelected(typeof tab === "string" ? tab : tab.id)
    }

    Binding {
        target: root
        property: "currentIndex"
        value: root.indexForId(root.selectedId)
        when: root.selectedId.length > 0
    }

    Repeater {
        model: root.tabs
        delegate: AppTabButton {
            required property var modelData
            objectName: typeof modelData !== "string" && modelData.objectName
                        ? modelData.objectName
                        : root.buttonObjectNamePrefix.length > 0
                        ? root.buttonObjectNamePrefix + modelData.id : ""
            text: typeof modelData === "string" ? modelData : modelData.label
            onClicked: root.tabSelected(typeof modelData === "string" ? modelData : modelData.id)
            Keys.onPressed: function(event) {
                if (event.key !== Qt.Key_Left && event.key !== Qt.Key_Right)
                    return
                const direction = (event.key === Qt.Key_Right ? 1 : -1)
                                  * (root.mirrored ? -1 : 1)
                root.selectTab((TabBar.index + direction + root.count) % root.count)
                event.accepted = true
            }
        }
    }
}
