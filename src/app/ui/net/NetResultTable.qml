pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import MiaCode.UI

Rectangle {
    id: root
    property var model
    required property var columns
    property bool interactive: true
    property bool reorderable: false
    property string wrappingRole: "artist"
    property int wrappingColumn: 3
    signal selectionChanged(string chartId, bool selected)
    signal previewRequested(string chartId)
    signal rowActivated(int row)
    signal rowDragStarted(int row)
    signal rowsDropped(int row)
    color: Theme.colors.background.elevated
    border.color: Theme.colors.border.control
    function columnWidth(index) {
        let fixed = 0, weight = 0
        for (const column of columns) { fixed += column.width || 0; weight += column.weight || 0 }
        return columns[index].width || Math.max(columns[index].minimum || 100,
            (width - fixed - 2 - verticalScroll.implicitWidth) * columns[index].weight / weight)
    }
    Flickable {
        id: scroll
        anchors.fill: parent
        anchors.margins: 1
        contentWidth: header.width + verticalScroll.implicitWidth
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: AppScrollBar { id: horizontalScroll }
        Row {
            id: header
            height: Math.max(36, Theme.controlMinHeight)
            Repeater {
                model: root.columns
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: root.columnWidth(index)
                    height: header.height
                    color: Theme.colors.background.panel
                    border.color: Theme.colors.border.control
                    Label { anchors.fill: parent; anchors.margins: 4; text: parent.modelData.title; color: Theme.colors.text.active; font.family: Theme.uiFont; font.pixelSize: Theme.uiFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                }
            }
        }
        ListView {
            id: rows
            y: header.height
            width: header.width
            height: Math.max(0, scroll.height - y - (scroll.contentWidth > scroll.width ? horizontalScroll.implicitHeight : 0))
            model: root.model
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {
                id: verticalScroll
                parent: scroll
                anchors.top: parent.top
                anchors.topMargin: header.height
                anchors.bottom: parent.bottom
                anchors.bottomMargin: scroll.contentWidth > scroll.width ? horizontalScroll.implicitHeight : 0
                anchors.right: parent.right
            }
            delegate: Row {
                id: tableRow
                required property int index
                required property var model
                width: rows.width
                height: Math.max(Theme.controlMinHeight + 8, measurement.implicitHeight + 12)
                Text { id: measurement; visible: false; width: root.columnWidth(root.wrappingColumn) - 12; text: tableRow.model[root.wrappingRole] || ""; font.family: Theme.uiFont; font.pixelSize: Theme.uiFontSize; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight }
                Repeater {
                    model: root.columns
                    delegate: Rectangle {
                        id: cell
                        required property int index
                        required property var modelData
                        width: root.columnWidth(index)
                        height: tableRow.height
                        color: tableRow.model.selected && !root.columns[1].checkbox ? Theme.colors.accent.focus
                            : tableRow.index % 2 ? Theme.colors.background.panel : Theme.colors.background.elevated
                        border.color: Theme.colors.border.control
                        DropArea {
                            anchors.fill: parent
                            enabled: root.reorderable && root.interactive
                            keys: ["net-upload-row"]
                            onDropped: drop => {
                                if (drop.source === root) {
                                    drop.acceptProposedAction()
                                    root.rowsDropped(tableRow.index + (drop.y > height / 2 ? 1 : 0))
                                }
                            }
                        }
                        Label {
                            anchors.fill: parent
                            anchors.margins: 6
                            visible: !cell.modelData.checkbox && !cell.modelData.preview
                            text: cell.modelData.role ? tableRow.model[cell.modelData.role] || "" : String(tableRow.index + 1)
                            color: Theme.colors.text.active
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.uiFontSize
                            wrapMode: cell.modelData.wrap ? Text.Wrap : Text.NoWrap
                            elide: Text.ElideRight
                            maximumLineCount: 3
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: cell.index === 0 ? Text.AlignHCenter : Text.AlignLeft
                            HoverHandler { id: hover }
                            ToolTip.visible: hover.hovered && truncated
                            ToolTip.text: text
                            TapHandler { enabled: root.interactive && !root.columns[1].checkbox; onTapped: root.rowActivated(tableRow.index) }
                        }
                        AppCheckBox { anchors.centerIn: parent; visible: !!cell.modelData.checkbox; checked: !!tableRow.model.selected; enabled: root.interactive; onClicked: root.selectionChanged(tableRow.model.chartId, checked) }
                        IconButton { anchors.centerIn: parent; visible: !!cell.modelData.preview; iconSource: Qt.resolvedUrl("icons/play.svg"); tooltip: qsTrId("net.online_preview"); enabled: root.interactive; onClicked: root.previewRequested(tableRow.model.chartId) }
                        Item {
                            id: dragToken
                            width: cell.width
                            height: cell.height
                            Drag.active: dragHandle.drag.active
                            Drag.source: root
                            Drag.keys: ["net-upload-row"]
                            Drag.hotSpot.x: width / 2
                            Drag.hotSpot.y: height / 2
                        }
                        MouseArea {
                            id: dragHandle
                            anchors.fill: parent
                            enabled: root.reorderable && root.interactive && cell.index === 0
                            cursorShape: Qt.OpenHandCursor
                            drag.target: dragToken
                            onPressed: root.rowDragStarted(tableRow.index)
                            onReleased: dragToken.Drag.drop()
                        }
                    }
                }
            }
        }
    }
}
