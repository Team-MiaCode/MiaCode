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
    signal tabSelected(string tabId)

    onCurrentIndexChanged: {
        if (visible && thumb.width > 0) {
            animateThumb = true
            thumbAnimReset.restart()
        }
    }

    Timer {
        id: thumbAnimReset
        interval: 160
        onTriggered: root.animateThumb = false
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
            visible: root.currentIndex >= 0 && root.segmentWidth > 0
            x: root.trackPadding + Math.max(0, root.currentIndex) * (root.segmentWidth + segmentRow.spacing)
            y: root.trackPadding
            width: root.segmentWidth
            height: Math.max(0, parent.height - 2 * root.trackPadding)
            radius: Math.max(2, root.radius - root.trackPadding)
            color: root.nativeMaterialActive
                ? (Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.88))
                : Theme.overlayColor(Theme.darkTheme
                    ? Theme.colors.buttonState.selected
                    : Theme.colors.background.panel)
            border.width: 1 / root.Screen.devicePixelRatio
            border.color: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.09)
            border.pixelAligned: false

            Behavior on x {
                enabled: root.animateThumb
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
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
                        const step = (event.key === Qt.Key_Right ? 1 : -1) * (root.mirrored ? -1 : 1)
                        root.selectIndex((segment.index + step + root.tabs.length) % root.tabs.length)
                        event.accepted = true
                    }

                    background: Rectangle {
                        radius: Math.max(2, root.radius - root.trackPadding)
                        color: segment.selected ? "transparent"
                            : segment.down
                                ? (Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0, 0, 0, 0.08))
                                : segment.hovered
                                    ? (Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.04))
                                    : "transparent"
                        border.width: segment.visualFocus ? Theme.controlBorderWidth : 0
                        border.color: Theme.colors.accent.focus
                        border.pixelAligned: false
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
