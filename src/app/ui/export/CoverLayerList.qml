pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

ListView {
    id: root

    required property var session
    required property var composerItem
    required property var layerName
    signal layerSelected(string key)
    property string draggedKey: ""
    property int dropRow: -1
    readonly property bool editable: !!session && !session.busy
    readonly property var orderedLayers: {
        const source = root.session ? root.session.layoutModel.layers : []
        const result = []
        for (let i = 0; i < source.length; ++i)
            result.push(source[i])
        return result.sort((a, b) => b.z - a.z)
    }

    function selectLayer(key) {
        root.forceActiveFocus()
        root.layerSelected(key)
    }

    function dropIndex(point) {
        const y = point.y + root.contentY
        const row = root.indexAt(point.x, y)
        if (row < 0)
            return y < root.contentY ? 0 : root.count
        const item = root.itemAtIndex(row)
        return row + (item && y > item.y + item.height / 2 ? 1 : 0)
    }

    function detail(layer) {
        let value = ""
        if (layer.kind === "image")
            value = String(layer.imagePath).split(/[\\/]/).pop()
        else if (layer.kind === "text")
            value = String(layer.text).replace(/\s+/g, " ")
        else if (layer.kind === "chartFrame")
            value = layer.frameSeconds.toFixed(2) + " s"
        const state = layer.visible ? Math.round(layer.opacity * 100) + "%"
                                    : qsTrId("cover.layer_hidden")
        return value.length > 0 ? value + " · " + state : state
    }

    model: root.orderedLayers
    spacing: 2
    clip: true
    currentIndex: {
        for (let i = 0; i < root.orderedLayers.length; ++i)
            if (root.session && root.orderedLayers[i].key === root.session.activeLayerKey)
                return i
        return -1
    }
    ScrollBar.vertical: AppScrollBar {}

    Keys.onPressed: function(event) {
        if (!root.editable || event.modifiers !== Qt.NoModifier || root.currentIndex < 0)
            return
        const layer = root.orderedLayers[root.currentIndex]
        if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
            root.session.removeActiveLayer()
        } else if (event.key === Qt.Key_V) {
            root.session.setLayerVisible(layer.key, !layer.visible)
        } else if (event.key === Qt.Key_L) {
            root.session.setLayerLocked(layer.key, !layer.locked)
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            const next = Math.max(0, Math.min(root.count - 1,
                root.currentIndex + (event.key === Qt.Key_Up ? -1 : 1)))
            root.selectLayer(root.orderedLayers[next].key)
            root.positionViewAtIndex(next, ListView.Contain)
        } else {
            return
        }
        event.accepted = true
    }

    delegate: ChromeRow {
        id: row
        required property var modelData
        required property int index
        readonly property Item previewSource: root.composerItem
            ? root.composerItem.layerPreviewItem(row.modelData.key, root.composerItem.layerPreviewRevision)
            : null
        width: ListView.view.width
        implicitHeight: 58
        rightPadding: controls.width + 6
        selected: !!root.session && root.session.activeLayerKey === row.modelData.key
        onClicked: root.selectLayer(row.modelData.key)
        Accessible.name: root.layerName(row.modelData)
        Accessible.description: root.detail(row.modelData)

        contentItem: RowLayout {
            spacing: 8
            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                color: Theme.colors.background.surface
                border.color: Theme.colors.border.normal
                radius: Theme.controlRadius
                ShaderEffectSource {
                    anchors.centerIn: parent
                    readonly property real aspect: row.previewSource && row.previewSource.height > 0
                        ? row.previewSource.width / row.previewSource.height : 1
                    width: Math.min(parent.width - 4, (parent.height - 4) * aspect)
                    height: width / aspect
                    sourceItem: row.previewSource
                    textureSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio),
                                         Math.ceil(height * Screen.devicePixelRatio))
                    live: row.visible
                    smooth: true
                    opacity: row.modelData.visible ? 1 : 0.45
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    text: root.layerName(row.modelData)
                    color: !row.modelData.visible ? Theme.colors.text.disabled
                        : row.selected ? Theme.colors.text.active : Theme.colors.text.primary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: root.detail(row.modelData)
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }
            }
            DragHandler {
                id: drag
                target: null
                enabled: root.editable
                acceptedButtons: Qt.LeftButton
                onActiveChanged: {
                    if (active) {
                        root.selectLayer(row.modelData.key)
                        root.draggedKey = row.modelData.key
                        root.dropRow = row.index
                    } else if (root.draggedKey.length > 0) {
                        const key = root.draggedKey
                        const destination = root.dropRow
                        root.draggedKey = ""
                        root.dropRow = -1
                        root.session.moveLayer(key, destination)
                    }
                }
                onTranslationChanged: {
                    if (active)
                        root.dropRow = root.dropIndex(parent.mapToItem(root, centroid.position))
                }
                onCanceled: {
                    root.draggedKey = ""
                    root.dropRow = -1
                }
            }
        }

        Row {
            id: controls
            parent: row
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            IconButton {
                compact: true
                enabled: root.editable
                iconSource: Qt.resolvedUrl(row.modelData.visible ? "icons/eye.svg" : "icons/eye-off.svg")
                tooltip: row.modelData.visible ? qsTrId("cover.hide") : qsTrId("cover.show")
                onClicked: root.session.setLayerVisible(row.modelData.key, !row.modelData.visible)
            }
            IconButton {
                compact: true
                enabled: root.editable
                iconSource: Qt.resolvedUrl(row.modelData.locked ? "icons/lock.svg" : "icons/lock-open.svg")
                tooltip: row.modelData.locked ? qsTrId("cover.unlock") : qsTrId("cover.lock")
                onClicked: root.session.setLayerLocked(row.modelData.key, !row.modelData.locked)
            }
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: root.editable
            onTapped: function(eventPoint) {
                root.selectLayer(row.modelData.key)
                menu.popup(row, eventPoint.position.x, eventPoint.position.y)
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            y: root.dropRow === root.count ? parent.height - height : 0
            height: 2
            color: Theme.colors.accent.primary
            visible: root.draggedKey.length > 0
                && (root.dropRow === row.index || (root.dropRow === root.count && row.index === root.count - 1))
        }
    }

    AppMenu {
        id: menu
        hugContent: true
        readonly property var layer: root.session ? root.session.activeLayer : null
        AppMenuItem {
            text: menu.layer && menu.layer.visible ? qsTrId("cover.hide") : qsTrId("cover.show")
            shortcutText: "V"
            onTriggered: if (menu.layer) root.session.setActiveLayerVisible(!menu.layer.visible)
        }
        AppMenuItem {
            text: menu.layer && menu.layer.locked ? qsTrId("cover.unlock") : qsTrId("cover.lock")
            shortcutText: "L"
            onTriggered: if (menu.layer) root.session.setActiveLayerLocked(!menu.layer.locked)
        }
        AppMenuSeparator {}
        AppMenuItem { text: qsTrId("cover.move_up"); onTriggered: root.session.raiseActiveLayer() }
        AppMenuItem { text: qsTrId("cover.move_down"); onTriggered: root.session.lowerActiveLayer() }
        AppMenuItem { text: qsTrId("cover.bring_to_front"); onTriggered: root.session.bringActiveLayerToFront() }
        AppMenuItem { text: qsTrId("cover.send_to_back"); onTriggered: root.session.sendActiveLayerToBack() }
        AppMenuSeparator {}
        AppMenuItem {
            text: qsTrId("cover.duplicate_layer")
            enabled: !!menu.layer && menu.layer.kind !== "card"
            onTriggered: root.session.duplicateActiveLayer()
        }
        AppMenuItem {
            text: qsTrId("cover.delete_layer")
            shortcutText: "Delete"
            enabled: !!menu.layer && menu.layer.kind !== "card"
            onTriggered: root.session.removeActiveLayer()
        }
    }
}
