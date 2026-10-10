import QtQuick
import QtQuick.Window
import MiaCode.UI

// Restore the backing pixels outside the arc in a corner-sized pass. The
// editor and live preview remain in the main scene, outside any texture layer.
ShaderEffect {
    id: root

    required property Item backgroundSource
    required property point backgroundOffset
    required property Item panelItem
    required property point panelOffset
    property real radius: Theme.workspaceRadius
    // This mask rounds the panel against the activity-bar strip, so its
    // restored pixels must use that strip's theme role.
    property color baseColor: Theme.colors.background.activityBar
    readonly property color surfaceColor: Theme.surfaceColor(baseColor)
    readonly property real nativeMaterial: Theme.nativeMaterialActive ? 1.0 : 0.0
    readonly property color nativeTintColor: Theme.chromeSurfaceColor(baseColor)
    readonly property color wallpaperBaseColor: Theme.colors.background.surface
    property color panelBackingColor: Theme.surfaceColor(Theme.colors.background.panel)
    property color separatorColor: Theme.separatorColor
    readonly property real separatorWidth: 1 / (Screen.devicePixelRatio * radius)
    // Replace only this corner-sized patch in the Quick surface. Its outside
    // pixels retain alpha so the system material remains visible underneath.
    blending: false
    readonly property var source: ShaderEffectSource {
        sourceItem: root.visible && !Theme.nativeMaterialActive ? root.backgroundSource : null
        sourceRect: Qt.rect(root.backgroundOffset.x, root.backgroundOffset.y,
                            root.width, root.height)
        textureSize: Qt.size(Math.ceil(root.width * root.Screen.devicePixelRatio),
                             Math.ceil(root.height * root.Screen.devicePixelRatio))
        visible: false
    }
    readonly property var panelSource: ShaderEffectSource {
        sourceItem: root.visible ? root.panelItem : null
        sourceRect: Qt.rect(root.panelOffset.x, root.panelOffset.y,
                            root.width, root.height)
        textureSize: Qt.size(Math.ceil(root.width * root.Screen.devicePixelRatio),
                             Math.ceil(root.height * root.Screen.devicePixelRatio))
        visible: false
    }

    width: radius
    height: radius
    enabled: false
    fragmentShader: "qrc:/src/app/ui/shaders/corner_mask.frag.qsb"
}
