import QtQuick
import QtQuick.Controls
import MiaCode.UI

Rectangle {
    id: root

    required property var hostWindow
    required property var windowChrome

    signal toggleSidebarRequested()
    signal toggleBottomRequested()
    signal openRequested()
    signal openCurrentFolderRequested()
    signal saveRequested()
    signal saveWholeDocumentRequested()
    signal undoRequested()
    signal redoRequested()
    signal audioSettingsRequested()
    signal previewSettingsRequested()
    signal showDetachedPreviewRequested()
    signal unavailableFeatureRequested(string featureName)

    property bool sidebarActive: false
    property bool bottomActive: false
    property bool saveEnabled: false
    property bool wholeDocumentSaveEnabled: false
    property bool currentFolderAvailable: false
    property bool bottomPanelEnabled: true
    property bool canUndo: false
    property bool canRedo: false
    property bool integratedInTitleBar: false
    property real titleBarLeadingInset: 0
    property bool previewSettingsVisible: true
    property real titleFullWidth: 0

    readonly property int buttonIconSize: Theme.toolbarButtonIconSize
    readonly property int buttonWidth: Theme.toolbarButtonWidth
    readonly property int buttonHeight: Theme.toolbarButtonHeight
    readonly property int buttonSpacing: Theme.toolbarButtonSpacing
    readonly property real titleGap: 16

    TextMetrics {
        id: audioSettingsMetrics
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        text: qsTrId("action.audio_settings")
    }

    TextMetrics {
        id: previewSettingsMetrics
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        text: qsTrId("action.video_settings")
    }

    TextMetrics {
        id: detachedPreviewMetrics
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        text: qsTrId("preview.window.show")
    }

    readonly property real expandedRightRowWidth: root.previewSettingsVisible
        ? Theme.toolbarLabeledButtonWidth(audioSettingsMetrics)
          + Theme.toolbarLabeledButtonWidth(previewSettingsMetrics)
          + 2 * root.buttonWidth + 3 * root.buttonSpacing
        : Theme.toolbarLabeledButtonWidth(detachedPreviewMetrics)
          + 2 * root.buttonWidth + 2 * root.buttonSpacing
    readonly property real collapsedRightRowWidth: (root.previewSettingsVisible ? 4 : 3) * root.buttonWidth
        + (root.previewSettingsVisible ? 3 : 2) * root.buttonSpacing
    readonly property real expandedTrailingActionsWidth: expandedRightRowWidth + rightActions.anchors.rightMargin
    readonly property real collapsedTrailingActionsWidth: collapsedRightRowWidth + rightActions.anchors.rightMargin
    readonly property bool settingsLabelsCollapsed: root.integratedInTitleBar
        ? root.titleFullWidth > Math.max(0,
            root.width - root.leadingActionsRight - root.expandedTrailingActionsWidth - 2 * root.titleGap)
        : root.width < root.leadingActionsRight + root.expandedTrailingActionsWidth + 16

    readonly property Item documentActionsItem: leftActions
    readonly property Item workspaceActionsItem: rightActions
    readonly property real leadingActionsRight: leftActions.x + leftActions.width
    readonly property real trailingActionsWidth: width - rightActions.x
    readonly property real minimumWidth: leftActions.anchors.leftMargin + leftActions.width
        + 16 + root.collapsedTrailingActionsWidth

    implicitHeight: root.integratedInTitleBar ? 32 : Theme.windowChromeRowHeight
    color: root.integratedInTitleBar
           ? "transparent"
           : Theme.chromeSurfaceColor(Theme.colors.background.activityBar)

    component ToolBarButton: IconButton {
        width: label.length > 0 ? implicitWidth : root.buttonWidth
        height: Math.min(root.buttonHeight, root.height)
        iconWidth: root.buttonIconSize
        iconHeight: root.buttonIconSize
        anchors.verticalCenter: parent.verticalCenter
        stateColors: Theme.chromeStateColors
    }

    WindowGestureArea {
        anchors.fill: parent
        hostWindow: root.hostWindow
        windowChrome: root.windowChrome
        visible: !root.integratedInTitleBar
        z: 0
    }

    Row {
        id: leftActions
        height: root.height
        anchors.left: parent.left
        anchors.leftMargin: root.integratedInTitleBar
                            ? (root.titleBarLeadingInset > 0
                               ? root.titleBarLeadingInset
                               : Theme.chromePadding)
                            : (Theme.activityButtonSize - openButton.width) / 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.buttonSpacing
        z: 1

        ToolBarButton {
            objectName: "openDocumentButton"
            id: openButton
            iconSource: Qt.resolvedUrl("icons/folder-open.svg")
            tooltip: qsTrId("action.open")
            onClicked: root.openRequested()
        }
        ToolBarButton {
            objectName: "openCurrentFolderButton"
            iconSource: Qt.resolvedUrl("icons/folder-arrow-right.svg")
            tooltip: qsTrId("action.open_current_folder")
            enabled: root.currentFolderAvailable
            onClicked: root.openCurrentFolderRequested()
        }
        ToolBarButton {
            objectName: "saveCurrentEditorButton"
            iconSource: Qt.resolvedUrl("icons/save.svg")
            tooltip: qsTrId("action.save")
            enabled: root.saveEnabled
            onClicked: root.saveRequested()
        }
        ToolBarButton {
            objectName: "saveWholeDocumentButton"
            iconSource: Qt.resolvedUrl("icons/save-multiple.svg")
            tooltip: qsTrId("qml.save_entire_document")
            enabled: root.wholeDocumentSaveEnabled
            onClicked: root.saveWholeDocumentRequested()
        }
        ToolBarButton {
            property bool preservesEditTarget: true
            iconSource: Qt.resolvedUrl("icons/undo.svg")
            tooltip: qsTrId("qml.undo")
            enabled: root.canUndo
            onClicked: root.undoRequested()
        }
        ToolBarButton {
            property bool preservesEditTarget: true
            iconSource: Qt.resolvedUrl("icons/redo.svg")
            tooltip: qsTrId("action.redo")
            enabled: root.canRedo
            onClicked: root.redoRequested()
        }
    }

    Row {
        id: rightActions
        height: root.height
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.buttonSpacing
        z: 1

        ToolBarButton {
            visible: root.previewSettingsVisible
            iconSource: Qt.resolvedUrl("icons/audio-settings.svg")
            label: root.settingsLabelsCollapsed ? "" : qsTrId("action.audio_settings")
            tooltip: qsTrId("action.audio_settings")
            onClicked: root.audioSettingsRequested()
        }
        ToolBarButton {
            visible: root.previewSettingsVisible
            iconSource: Qt.resolvedUrl("icons/preview-settings.svg")
            label: root.settingsLabelsCollapsed ? "" : qsTrId("action.video_settings")
            tooltip: qsTrId("action.video_settings")
            onClicked: root.previewSettingsRequested()
        }
        ToolBarButton {
            objectName: "showDetachedPreviewButton"
            visible: !root.previewSettingsVisible
            iconSource: Qt.resolvedUrl("icons/preview-detach.svg")
            label: root.settingsLabelsCollapsed ? "" : qsTrId("preview.window.show")
            tooltip: qsTrId("preview.window.show")
            onClicked: root.showDetachedPreviewRequested()
        }
        ToolBarButton {
            objectName: "toggleSidebarButton"
            iconSource: Qt.resolvedUrl("icons/panel-left.svg")
            filledIconSource: Qt.resolvedUrl("icons/panel-left-fill.svg")
            tooltip: root.sidebarActive ? qsTrId("qml.close_sidebar") : qsTrId("qml.open_sidebar")
            highlightWhenActive: false
            active: root.sidebarActive
            onClicked: root.toggleSidebarRequested()
        }
        ToolBarButton {
            iconSource: Qt.resolvedUrl("icons/panel-bottom.svg")
            filledIconSource: Qt.resolvedUrl("icons/panel-bottom-fill.svg")
            tooltip: root.bottomActive ? qsTrId("qml.close_bottom_panel") : qsTrId("qml.open_bottom_panel")
            highlightWhenActive: false
            enabled: root.bottomPanelEnabled
            active: root.bottomActive
            onClicked: root.toggleBottomRequested()
        }
    }

}
