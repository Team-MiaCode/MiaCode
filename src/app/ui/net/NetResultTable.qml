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
    color: Theme.surfaceColor(Theme.colors.background.surface)
    border.color: Theme.colors.border.normal

    FontMetrics {
        id: headerMetrics
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        font.weight: Font.Medium
    }

    // Keep wrapped row heights independent of the horizontal scroll range.
    readonly property real verticalScrollExtent: rows.count > 0
        ? verticalScroll.implicitWidth : 0
    readonly property var columnWidths: {
        if (!columns || columns.length === 0)
            return []
        const minimums = columns.map(column => Math.max(
            column.width || column.minimum || 100,
            Math.ceil(headerMetrics.advanceWidth(column.title)) + 16))
        const minimumTotal = minimums.reduce((sum, value) => sum + value, 0)
        const weightTotal = columns.reduce((sum, column) => sum + (column.weight || 0), 0)
        const extra = Math.max(0, Math.floor(scroll.width - verticalScrollExtent) - minimumTotal)
        let accumulatedWeight = 0
        let assignedExtra = 0
        return columns.map((column, index) => {
            if (!column.weight || weightTotal === 0)
                return minimums[index]
            accumulatedWeight += column.weight
            const through = Math.floor(extra * accumulatedWeight / weightTotal)
            const value = minimums[index] + through - assignedExtra
            assignedExtra = through
            return value
        })
    }

    function columnWidth(index) {
        return columnWidths[index] || 0
    }
    Flickable {
        id: scroll
        anchors.fill: parent
        anchors.margins: 1
        contentWidth: Math.max(width, header.width + root.verticalScrollExtent)
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: AppScrollBar {
            id: horizontalScroll
            policy: scroll.contentWidth > scroll.width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }
        Row {
            id: header
            height: Math.max(32, Theme.controlMinHeight)
            Repeater {
                model: root.columns
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: root.columnWidth(index)
                    height: header.height
                    color: Theme.surfaceColor(Theme.colors.background.panel)
                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        text: parent.modelData.title
                        color: Theme.colors.text.secondary
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.secondaryFontSize
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.separatorColor
                    }
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 1
                        visible: index < root.columns.length - 1
                        color: Theme.separatorColor
                    }
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
                policy: rows.contentHeight > rows.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }
            delegate: Row {
                id: tableRow
                required property int index
                required property var model
                width: rows.width
                height: Math.max(Theme.controlMinHeight + 8, measurement.implicitHeight + 16)
                HoverHandler { id: rowHover }
                Text { id: measurement; visible: false; width: root.columnWidth(root.wrappingColumn) - 16; text: tableRow.model[root.wrappingRole] || ""; font.family: Theme.uiFont; font.pixelSize: Theme.uiFontSize; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight }
                Repeater {
                    model: root.columns
                    delegate: Rectangle {
                        id: cell
                        required property int index
                        required property var modelData
                        width: root.columnWidth(index)
                        height: tableRow.height
                        color: tableRow.model.selected && !root.columns[1].checkbox
                            ? Qt.tint(Theme.surfaceColor(Theme.colors.background.surface),
                                      Theme.contentOverlayColor(Theme.colors.accent.primary, 0.14))
                            : rowHover.hovered ? Theme.surfaceColor(Theme.colors.background.panel)
                            : Theme.surfaceColor(Theme.colors.background.surface)
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Theme.separatorColor
                        }
                        Rectangle {
                            width: 2
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            visible: cell.index === 0 && tableRow.model.selected && !root.columns[1].checkbox
                            color: Theme.colors.accent.primary
                        }
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
                            anchors.margins: 8
                            visible: !cell.modelData.checkbox && !cell.modelData.preview
                            text: cell.modelData.role ? tableRow.model[cell.modelData.role] || "" : String(tableRow.index + 1)
                            color: cell.index === 0 ? Theme.colors.text.secondary : Theme.colors.text.primary
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.uiFontSize
                            wrapMode: cell.modelData.wrap ? Text.Wrap : Text.NoWrap
                            elide: Text.ElideRight
                            maximumLineCount: 3
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: cell.modelData.alignment !== undefined
                                ? cell.modelData.alignment
                                : cell.index === 0 ? Text.AlignHCenter : Text.AlignLeft
                            HoverHandler { id: hover }
                            ToolTip.visible: hover.hovered && (truncated || !!cell.modelData.tooltipRole)
                            ToolTip.text: cell.modelData.tooltipRole ? tableRow.model[cell.modelData.tooltipRole] || text : text
                            TapHandler { enabled: root.interactive && !root.columns[1].checkbox; onTapped: root.rowActivated(tableRow.index) }
                        }
                        AppCheckBox { anchors.centerIn: parent; visible: !!cell.modelData.checkbox; checked: !!tableRow.model.selected; enabled: root.interactive; onClicked: root.selectionChanged(tableRow.model.chartId, checked) }
                        NetPageButton { anchors.centerIn: parent; visible: !!cell.modelData.preview; iconSource: Qt.resolvedUrl("icons/play.svg"); tooltip: qsTrId("net.online_preview"); enabled: root.interactive; onClicked: root.previewRequested(tableRow.model.chartId) }
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
