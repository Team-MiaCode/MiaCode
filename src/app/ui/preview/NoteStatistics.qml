pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

// Note counts under the preview. Column count follows whether six
// value cells fit; never elide numeric values. Transport chrome is
// independent — do not share a width threshold with PreviewTransport.
Item {
    id: root

    required property var statistics
    property bool dataAvailable: true
    property bool efficientMode: false

    readonly property int iconSlotWidth: 24
    readonly property int contentSpacing: 6
    readonly property real textColumnWidth: Math.max(nameWidthProbe.implicitWidth,
                                                     valueWidthProbe.implicitWidth)
    readonly property real contentWidth: iconSlotWidth + contentSpacing + textColumnWidth
    // The row can contract its text track; keep the six-column breakpoint compact.
    readonly property int minCellWidth: 88
    readonly property bool wideLayout: width >= minCellWidth * 6
    readonly property int columns: wideLayout ? 6 : 3
    readonly property int rows: wideLayout ? 1 : 2

    implicitHeight: efficientMode ? 76 : wideLayout ? 55 : 96
    height: implicitHeight

    Text {
        id: nameWidthProbe
        visible: false
        text: qsTrId("note_stats.touch")
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
    }

    Text {
        id: valueWidthProbe
        visible: false
        text: "8888/8888"
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
    }

    Grid {
        id: grid
        visible: !root.efficientMode
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        columns: root.columns
        columnSpacing: 0
        rowSpacing: 0

        Repeater {
            // 统计值刷新会替换 QVariantList；数量保持稳定时复用现有单元，
            // 让数值绑定更新，保持图标、文字与布局对象的生命周期连续。
            model: root.statistics.length

            delegate: Item {
                id: cell
                required property int index
                readonly property var modelData: root.statistics[index]

                width: grid.width / root.columns
                height: grid.height / root.rows

                Row {
                    anchors.centerIn: parent
                    width: Math.min(root.contentWidth, cell.width - 8)
                    height: Math.max(28, textColumn.implicitHeight)
                    spacing: root.contentSpacing

                    Item {
                        width: root.iconSlotWidth
                        height: 28
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            anchors.centerIn: parent
                            width: cell.modelData.kind === "hold" ? 18
                                : cell.modelData.kind === "slide" ? 24 : 20
                            height: cell.modelData.kind === "hold" ? 26 : width
                            visible: cell.modelData.kind !== "total"
                            source: cell.modelData.iconSource
                            sourceSize: Theme.iconSourceSize(width, height)
                            fillMode: cell.modelData.kind === "hold"
                                ? Image.Stretch : Image.PreserveAspectFit
                            smooth: true
                        }

                        ControlsImpl.IconImage {
                            anchors.centerIn: parent
                            width: 26
                            height: 26
                            visible: cell.modelData.kind === "total"
                            source: Qt.resolvedUrl("icons/chart.svg")
                            sourceSize: Theme.iconSourceSize(width, height)
                            color: Theme.colors.text.secondary
                            anchors.verticalCenterOffset: -1
                        }
                    }

                    Column {
                        id: textColumn
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        width: parent.width - root.iconSlotWidth - parent.spacing

                        Text {
                            id: nameLabel
                            width: parent.width
                            text: cell.modelData.name
                            color: Theme.colors.text.secondary
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.secondaryFontSize
                            horizontalAlignment: Text.AlignLeft
                            elide: Text.ElideRight
                        }
                        Text {
                            id: valueLabel
                            width: parent.width
                            // 数值水平缩字时维持行槽高度，名称和图标的中心位置稳定。
                            height: valueWidthProbe.implicitHeight
                            text: root.dataAvailable ? cell.modelData.value : "—"
                            color: Theme.colors.text.primary
                            font.family: Theme.uiFont
                            font.pixelSize: root.wideLayout ? Theme.uiFontSize : Theme.secondaryFontSize
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: Math.max(1, Theme.secondaryFontSize - 1)
                        }
                    }
                }
            }
        }
    }

    Grid {
        id: textGrid
        visible: root.efficientMode
        anchors.fill: parent
        anchors.margins: 6
        columns: 3
        columnSpacing: 6
        rowSpacing: 6

        Repeater {
            model: root.efficientMode ? root.statistics.length : 0

            delegate: Rectangle {
                id: textCell
                required property int index
                readonly property var modelData: root.statistics[index]
                width: (textGrid.width - 2 * textGrid.columnSpacing) / 3
                height: (textGrid.height - textGrid.rowSpacing) / 2
                radius: Theme.controlRadius
                color: Theme.overlayColor(Theme.colors.background.control)
                border.width: 1
                border.color: modelData.kind === "total"
                    ? Theme.colors.accent.focus : Theme.separatorColor

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    text: textCell.modelData.name + "  "
                        + (root.dataAvailable ? textCell.modelData.value : "—")
                    color: root.enabled ? Theme.colors.text.primary : Theme.colors.text.disabled
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: Math.max(1, Theme.secondaryFontSize - 1)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

}
