import QtQuick
import QtQuick.Controls
import MiaCode.UI

ApplicationWindow {
    id: window

    required property var controller
    required property var coverSession
    required property var preferences
    required property var platform
    required property var windowChrome
    readonly property Item backdropSource: page
    readonly property bool nativeMaterialActive: windowChrome.nativeMaterialAvailable
        && Theme.blurMaterialsEnabled && !Theme.backgroundActive

    title: qsTrId("cover.window.title")
    flags: {
        let value = Qt.Window
        if (window.platform.captionButtons)
            value |= Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
                | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
        if (Qt.platform.os === "linux")
            value |= Qt.FramelessWindowHint
        if (Qt.platform.os === "osx")
            value |= Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
        return value
    }
    modality: Qt.NonModal
    transientParent: null
    width: 1280
    height: 800
    minimumWidth: page.implicitWidth
    minimumHeight: 480 + titleBar.height
    visible: false
    color: Qt.platform.os === "osx" || Qt.platform.os === "windows"
        ? "transparent" : Theme.colors.background.surface
    background: null
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    font.family: Theme.uiFont
    font.pixelSize: Theme.uiFontSize

    palette.window: Theme.colors.background.surface
    palette.windowText: Theme.colors.text.primary
    palette.base: Theme.colors.background.surface
    palette.text: Theme.colors.text.primary
    palette.button: Theme.colors.background.panel
    palette.buttonText: Theme.colors.text.primary
    palette.highlight: Theme.colors.state.textSelection
    palette.highlightedText: Theme.colors.text.active
    palette.placeholderText: Theme.colors.text.secondary
    palette.disabled.text: Theme.colors.text.disabled
    palette.disabled.buttonText: Theme.colors.text.disabled

    Binding {
        target: Theme
        property: "preferences"
        value: window.preferences
    }

    onClosing: function(event) {
        event.accepted = !window.coverSession.busy
        if (event.accepted)
            window.controller.close()
    }

    Binding {
        target: window.windowChrome
        property: "materialRegions"
        when: Qt.platform.os === "osx" || Qt.platform.os === "windows"
        value: window.visible && titleBar.visible
            && Theme.blurMaterialsEnabled && !Theme.backgroundActive
            ? [{ rect: Qt.rect(0, 0, window.width, titleBar.height) }] : []
        restoreMode: Binding.RestoreNone
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
        nativeHeight: window.windowChrome.titleBarHeight
        leadingInset: window.windowChrome.titleBarLeadingInset
        leadingToolAreaWidth: leadingInset
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

    Loader {
        anchors.fill: parent
        active: Qt.platform.os === "linux"
        sourceComponent: WindowResizeBorder { hostWindow: window }
    }

    Shortcut {
        sequence: StandardKey.Close
        onActivated: window.close()
    }
}
