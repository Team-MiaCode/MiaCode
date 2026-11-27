import QtQuick
import QtQuick.Controls
import MiaCode.UI

ApplicationWindow {
    id: root

    required property var platform
    required property var windowChrome
    property bool staysOnTop: false
    property color fallbackColor: Theme.colors.background.surface
    property bool manageTopMaterialRegion: false
    property real topMaterialThickness: 0
    readonly property alias backdropSource: sceneContent
    readonly property alias backgroundLayer: backgroundLayer
    readonly property bool nativeMaterialActive: root.windowChrome !== null
        && root.windowChrome.nativeMaterialAvailable && Theme.blurMaterialsEnabled
        && !Theme.backgroundActive
    default property alias windowContent: contentHost.data

    flags: {
        let value = Qt.Window
        if (Qt.platform.os !== "windows" && root.staysOnTop)
            value |= Qt.WindowStaysOnTopHint
        if (root.platform && root.platform.captionButtons) {
            value |= Qt.CustomizeWindowHint
                    | Qt.WindowTitleHint
                    | Qt.WindowSystemMenuHint
                    | Qt.WindowMinimizeButtonHint
                    | Qt.WindowMaximizeButtonHint
                    | Qt.WindowCloseButtonHint
        }
        if (Qt.platform.os === "linux")
            value |= Qt.FramelessWindowHint
        if (Qt.platform.os === "osx")
            value |= Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
        return value
    }
    // Allocate an alpha-capable surface at startup and retain it across
    // wallpaper/material switches. Opaque modes are painted by the base layer.
    color: Qt.platform.os === "osx" || Qt.platform.os === "windows"
        ? "transparent" : root.fallbackColor
    background: null
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    font.family: Theme.uiFont
    font.pixelSize: Theme.uiFontSize

    palette.window: Theme.colors.background.surface
    palette.windowText: Theme.colors.text.primary
    palette.base: Theme.overlayColor(Theme.colors.background.surface)
    palette.alternateBase: Theme.overlayColor(Theme.colors.background.elevated)
    palette.text: Theme.colors.text.primary
    palette.button: Theme.overlayColor(Theme.colors.background.panel)
    palette.buttonText: Theme.colors.text.primary
    palette.light: Theme.colors.border.control
    palette.midlight: Theme.colors.border.normal
    palette.mid: Theme.colors.border.control
    palette.dark: Theme.colors.background.surface
    palette.shadow: "#000000"
    palette.highlight: Theme.overlayColor(Theme.colors.state.textSelection)
    palette.highlightedText: Theme.colors.text.active
    palette.placeholderText: Theme.colors.text.secondary
    palette.toolTipBase: Theme.overlayColor(Theme.colors.background.elevated, Theme.popupOpacity)
    palette.toolTipText: Theme.colors.text.primary
    palette.disabled.text: Theme.colors.text.disabled
    palette.disabled.windowText: Theme.colors.text.disabled
    palette.disabled.buttonText: Theme.colors.text.disabled

    Binding {
        target: root.windowChrome
        property: "materialRegions"
        when: root.manageTopMaterialRegion
            && (Qt.platform.os === "osx" || Qt.platform.os === "windows")
            && root.windowChrome !== null
        value: root.visible && root.topMaterialThickness > 0
            && Theme.blurMaterialsEnabled && !Theme.backgroundActive
            ? [{ edge: "top", thickness: Math.round(root.topMaterialThickness * Theme.uiScale) }]
            : []
        restoreMode: Binding.RestoreNone
    }

    UiScaleSurface {
        id: sceneContent

        WindowBackground {
            id: backgroundLayer
            anchors.fill: parent
            nativeMaterialActive: root.nativeMaterialActive
        }

        Item {
            id: contentHost
            anchors.fill: parent
        }
    }

    Loader {
        active: Qt.platform.os === "linux"
        anchors.fill: parent
        sourceComponent: WindowResizeBorder {
            hostWindow: root
        }
    }
}
