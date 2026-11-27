pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

// Compact equal-width segmented switcher with a sliding active pill.
// Supports both opaque panels and translucent native vibrancy surfaces.
Item {
    id: root

    property var tabs: []
    property string selectedId: ""
    property bool nativeMaterialActive: false
    property real radius: Theme.controlRadius
    property real trackPadding: 2
    property bool animateThumb: false
    property bool animationsReady: false
    signal tabSelected(string tabId)

    function suppressThumbAnimation() {
        animationsReady = false
        animateThumb = false
        Qt.callLater(() => root.animationsReady = true)
    }
    Component.onCompleted: Qt.callLater(() => root.animationsReady = true)
    onWidthChanged: suppressThumbAnimation()
    onSegmentWidthChanged: suppressThumbAnimation()
    onRightToLeftChanged: suppressThumbAnimation()

    onCurrentIndexChanged: {
        animateThumb = animationsReady && visible && thumb.width > 0 && Theme.animationsEnabled
        Qt.callLater(() => { if (!thumbMotion.running) root.animateThumb = false })
    }

    readonly property int currentIndex: {
        for (let i = 0; i < tabs.length; ++i) {
            const tab = tabs[i]
            const id = typeof tab === "string" ? tab : tab.id
            if (id === selectedId)
                return i
        }
        return tabs.length > 0 ? 0 : -1
    }
    readonly property real segmentWidth: tabs.length > 0
        ? Math.max(0, (track.width - 2 * root.trackPadding - Math.max(0, tabs.length - 1) * segmentRow.spacing) / tabs.length)
        : 0
    readonly property bool rightToLeft: segmentRow.effectiveLayoutDirection === Qt.RightToLeft

    implicitHeight: Theme.controlMinHeight
    implicitWidth: Math.max(120, tabs.length * 76 + 2 * trackPadding)
    Accessible.role: Accessible.PageTabList

    function selectIndex(index) {
        if (index < 0 || index >= tabs.length)
            return
        const tab = tabs[index]
        const id = typeof tab === "string" ? tab : tab.id
        const item = segments.itemAt(index)
        if (item)
            item.forceActiveFocus(Qt.TabFocusReason)
        tabSelected(id)
    }

    Rectangle {
        id: track
        anchors.fill: parent
        radius: root.radius
        color: root.nativeMaterialActive
            ? (Theme.darkTheme ? Qt.rgba(0, 0, 0, 0.22) : Qt.rgba(0, 0, 0, 0.06))
            : Theme.overlayColor(Theme.colors.background.control)
        border.width: 1 / root.Screen.devicePixelRatio
        border.color: root.nativeMaterialActive
            ? (Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.08))
            : Theme.colors.border.normal
        border.pixelAligned: false

        // Sliding active pill with concentric corner radius.
        Rectangle {
            id: thumb
            readonly property int visualIndex: root.rightToLeft
                ? root.tabs.length - 1 - root.currentIndex
                : root.currentIndex
            visible: root.currentIndex >= 0 && root.segmentWidth > 0
            x: root.trackPadding + Math.max(0, visualIndex) * (root.segmentWidth + segmentRow.spacing)
            y: root.trackPadding
            width: root.segmentWidth
            height: Math.max(0, parent.height - 2 * root.trackPadding)
            radius: Math.max(2, root.radius - root.trackPadding)
            color: root.nativeMaterialActive
                ? Theme.chromeStateColors.selected
                : Theme.overlayColor(Theme.darkTheme
                    ? Theme.colors.buttonState.selected
                    : Theme.colors.background.elevated)
            border.width: 1 / root.Screen.devicePixelRatio
            border.color: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.09)
            border.pixelAligned: false

            Behavior on x {
                enabled: root.animateThumb && Theme.animationsEnabled
                NumberAnimation { id: thumbMotion; duration: Theme.tabSlideDuration; easing.type: Easing.OutCubic }
            }
        }

        Row {
            id: segmentRow
            anchors.fill: parent
            anchors.margins: root.trackPadding
            spacing: 2

            Repeater {
                id: segments
                model: root.tabs

                delegate: AbstractButton {
                    id: segment

                    required property int index
                    required property var modelData
                    readonly property string tabId: typeof modelData === "string" ? modelData : modelData.id
                    readonly property bool selected: index === root.currentIndex

                    objectName: typeof modelData !== "string" && modelData.objectName ? modelData.objectName : ""
                    width: root.segmentWidth
                    height: segmentRow.height
                    text: typeof modelData === "string" ? modelData : modelData.label
                    hoverEnabled: true
                    activeFocusOnTab: true
                    Accessible.role: Accessible.PageTab
                    Accessible.name: segment.text

                    onClicked: root.tabSelected(tabId)
                    Keys.onPressed: event => {
                        if (event.key !== Qt.Key_Left && event.key !== Qt.Key_Right)
                            return
                        const step = (event.key === Qt.Key_Right ? 1 : -1) * (root.rightToLeft ? -1 : 1)
                        root.selectIndex((segment.index + step + root.tabs.length) % root.tabs.length)
                        event.accepted = true
                    }

                    background: HoverChrome {
                        fillBounds: true
                        highlightOutset: 0
                        cornerRadius: Math.max(2, root.radius - root.trackPadding)
                        stateColors: root.nativeMaterialActive ? Theme.chromeStateColors : Theme.colors.buttonState
                        hovered: !segment.selected && segment.hovered
                        pressed: !segment.selected && segment.down
                        focused: segment.visualFocus
                    }

                    contentItem: Text {
                        text: segment.text
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        font.weight: segment.selected ? Font.DemiBold : Font.Normal
                        color: segment.selected ? Theme.colors.text.active
                             : segment.hovered ? Theme.colors.text.primary
                             : Theme.colors.text.secondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 6
                        rightPadding: 6
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
