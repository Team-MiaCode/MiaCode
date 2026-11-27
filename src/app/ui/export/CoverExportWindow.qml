import QtQuick
import QtQuick.Controls
import MiaCode.UI

AppWindow {
    id: window

    required property var controller
    required property var coverSession
    required property var preferences
    required property var shortcuts

    title: qsTrId("cover.window.title")
    modality: Qt.NonModal
    transientParent: null
    width: 1280
    height: 800
    minimumWidth: Math.min(Screen.desktopAvailableWidth, Math.ceil(page.implicitWidth * Theme.uiScale))
    minimumHeight: Math.min(Screen.desktopAvailableHeight, Math.ceil((480 + titleBar.height) * Theme.uiScale))
    visible: false
    manageTopMaterialRegion: true
    topMaterialThickness: titleBar.visible ? titleBar.height : 0

    Binding {
        target: Theme
        property: "preferences"
        value: window.preferences
    }

    UiScaleShortcuts {
        shortcuts: window.shortcuts
        preferencesModel: window.preferences
    }

    onClosing: function(event) {
        event.accepted = !window.coverSession.busy
        if (event.accepted)
            window.controller.close()
    }

    WindowTitleBar {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: window.visibility !== Window.FullScreen
        height: visible ? implicitHeight : 0
        hostWindow: window
        windowChrome: window.windowChrome
        platform: window.platform
        applicationMenusVisible: false
        titleText: window.title
        color: Theme.chromeSurfaceColor(Theme.colors.background.titleBar,
            window.nativeMaterialActive)
    }

    CoverExportPage {
        id: page
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        coverSession: window.coverSession
    }

    UiRequestHost {
        requests: window.coverSession.uiRequests
    }

    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: window.close()
    }
}
