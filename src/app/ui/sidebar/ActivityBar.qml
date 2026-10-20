import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Rectangle {
    id: root

    property string activeView: "chart"
    property bool documentAvailable: true
    property bool netEnabled: false
    property bool networkActive: false
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
            id: majnetButton
            objectName: "majnetActivityButton"
            iconSource: Qt.resolvedUrl("icons/globe.svg")
            tooltip: qsTrId("net.ui.majnet")
            enabled: root.netEnabled
            selected: root.networkActive || majnetPopup.active
            onClicked: majnetPopup.popup(majnetButton, majnetButton.width, 0)
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

    // Keep the left navigation entry and the top-level Majnet menu identical.
    MajnetActionsMenu {
        id: majnetPopup
        actionsEnabled: root.netEnabled
        onDownloadRequested: root.toolRequested("net-download")
        onUploadRequested: root.toolRequested("net-upload")
    }

    AppMenu {
        id: toolsPopup
        hugContent: true

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

        // Controls size contentItem to the button's available area. Keep the
        // icon inside a container so its higher-resolution source cannot
        // change its painted size relative to the selection chrome.
        contentItem: Item {
            ControlsImpl.IconImage {
                objectName: "activityButtonIcon"
                anchors.centerIn: parent
                width: Theme.activityIconSize
                height: Theme.activityIconSize
                source: button.selected && button.filledIconSource.toString().length > 0
                    ? button.filledIconSource : button.iconSource
                sourceSize: Theme.iconSourceSize(Theme.activityIconSize, Theme.activityIconSize)
                color: !button.enabled ? Theme.colors.text.disabled
                     : button.selected ? Theme.colors.activityIcon.active
                     : button.hovered ? Theme.colors.activityIcon.hover
                     : Theme.colors.activityIcon.idle
            }
        }

        background: Item {
            HoverChrome {
                objectName: "activityButtonChrome"
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
