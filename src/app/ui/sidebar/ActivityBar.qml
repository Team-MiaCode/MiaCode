import QtQuick
import QtQml.Models
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Rectangle {
    id: root

    property string activeView: "chart"
    property bool documentAvailable: true
    property bool netEnabled: false
    property bool toolsAvailable: true
    property bool chartEditorAvailable: true
    property bool normalizationEnabled: true
    readonly property bool toolsMenuActive: toolsPopup.active
    signal viewRequested(string viewId)
    signal toolRequested(string toolId)
    signal settingsRequested()

    function showMediaToolsMenu(anchorItem) {
        if (root.toolsAvailable)
            toolsPopup.popup(anchorItem || toolsButton, (anchorItem || toolsButton).width, 0)
    }

    implicitWidth: Theme.activityButtonSize
    color: Theme.surfaceColor(Theme.colors.background.activityBar)

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        ActivityButton {
            height: 2 * Theme.workspaceHeaderContentCenterY
            iconSource: Qt.resolvedUrl("icons/file.svg")
            filledIconSource: Qt.resolvedUrl("icons/file-fill.svg")
            tooltip: qsTrId("qml.chart")
            selected: root.activeView === "chart"
            onClicked: root.viewRequested("chart")
        }
        ActivityButton {
            iconSource: Qt.resolvedUrl("icons/export.svg")
            filledIconSource: Qt.resolvedUrl("icons/export-fill.svg")
            tooltip: qsTrId("sidebar.export")
            enabled: root.documentAvailable
            selected: root.activeView === "export"
            onClicked: root.viewRequested("export")
        }
        ActivityButton {
            id: toolsButton
            iconSource: Qt.resolvedUrl("icons/tools.svg")
            filledIconSource: Qt.resolvedUrl("icons/tools-fill.svg")
            tooltip: qsTrId("qml.tools")
            enabled: root.toolsAvailable
            selected: toolsPopup.active
            onClicked: {
                if (toolsPopup.active)
                    toolsPopup.close()
                else
                    toolsPopup.popup(toolsButton, toolsButton.width, 0)
            }
        }
    }

    AppMenu {
        id: toolsPopup
        hugContent: true

        Instantiator {
            model: root.netEnabled ? ["net-download", "net-upload"] : []
            delegate: AppMenuAction {
                required property string modelData
                text: qsTrId(modelData === "net-download" ? "net.ui.download_page" : "net.ui.upload_page")
                enabled: root.toolsAvailable
                onTriggered: root.toolRequested(modelData)
            }
            onObjectAdded: (index, object) => toolsPopup.insertAction(index, object)
            onObjectRemoved: (index, object) => toolsPopup.removeAction(object)
        }
        AudioProcessingMenu {
            enabled: root.toolsAvailable
            documentAvailable: root.documentAvailable
            onToolRequested: toolId => root.toolRequested("media." + toolId)
        }
        VideoProcessingMenu {
            enabled: root.toolsAvailable
            documentAvailable: root.documentAvailable
            onToolRequested: toolId => root.toolRequested("media." + toolId)
        }
        AppMenuAction {
            text: qsTrId("qml.normalize_whole_chart")
            enabled: root.chartEditorAvailable && root.normalizationEnabled
            onTriggered: root.toolRequested("normalize")
        }
    }

    onToolsAvailableChanged: {
        if (!toolsAvailable)
            toolsPopup.close()
    }

    ActivityButton {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        iconSource: Qt.resolvedUrl("icons/settings.svg")
        tooltip: qsTrId("qml.view_settings")
        onClicked: root.settingsRequested()
    }

    component ActivityButton: AbstractButton {
        id: button

        required property url iconSource
        property url filledIconSource
        required property string tooltip
        property bool selected: false

        width: Theme.activityButtonSize
        height: Theme.activityButtonSize
        hoverEnabled: true

        contentItem: ControlsImpl.IconImage {
            anchors.centerIn: parent
            width: Theme.activityIconSize
            height: Theme.activityIconSize
            source: button.selected && button.filledIconSource.toString().length > 0
                ? button.filledIconSource : button.iconSource
            sourceSize: Qt.size(Theme.activityIconSize, Theme.activityIconSize)
            color: !button.enabled ? Theme.colors.text.disabled
                 : button.selected ? Theme.colors.activityIcon.active
                 : button.hovered ? Theme.colors.activityIcon.hover
                 : Theme.colors.activityIcon.idle
        }

        background: Item {
            HoverChrome {
                anchors.fill: parent
                contentWidth: Theme.activityIconSize + 1
                contentHeight: Theme.activityIconSize
                stateColors: Theme.chromeStateColors
                hovered: button.hovered
                pressed: button.down
                selected: button.selected
            }

        }

        Tooltip {
            visible: button.hovered
            text: button.tooltip
        }
    }
}
