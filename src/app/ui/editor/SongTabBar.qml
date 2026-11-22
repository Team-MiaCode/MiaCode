pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import MiaCode.UI

Flickable {
    id: root
    required property var documentSession
    required property var previewSession
    visible: documentSession.songTabs.length > 0
    implicitHeight: Theme.workspaceHeaderHeight
    contentWidth: tabs.width
    contentHeight: height
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.HorizontalFlick
    ScrollBar.horizontal: AppScrollBar {}

    function revealActive() {
        for (let index = 0; index < songItems.count; ++index) {
            const item = songItems.itemAt(index)
            if (!item?.active)
                continue
            if (item.x < contentX)
                contentX = item.x
            else if (item.x + item.width > contentX + width)
                contentX = Math.max(0, item.x + item.width - width)
            break
        }
    }

    onWidthChanged: Qt.callLater(revealActive)
    onVisibleChanged: {
        if (visible)
            Qt.callLater(revealActive)
    }

    Row {
        id: tabs
        Repeater {
            id: songItems
            model: root.documentSession.songTabs
            AppTab {
                required property var modelData
                width: preferredTabWidth
                height: root.height
                text: modelData.title || qsTrId("library.untitled")
                secondaryText: modelData.dirty ? "●" : ""
                tooltip: modelData.path
                active: modelData.id === root.documentSession.activeSongId
                onActiveChanged: if (active) Qt.callLater(root.revealActive)
                Component.onCompleted: if (active) Qt.callLater(root.revealActive)
                closable: true
                onClicked: {
                    if (!active && root.previewSession.playing)
                        root.previewSession.playing = false
                    root.documentSession.activateSong(modelData.id)
                }
                onCloseRequested: root.documentSession.requestCloseSong(modelData.id)
            }
        }
    }
}
