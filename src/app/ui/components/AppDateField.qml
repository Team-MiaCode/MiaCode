pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root
    property date selectedDate: new Date()
    readonly property string text: Qt.formatDate(selectedDate, "yyyy-MM-dd")
    implicitWidth: 158
    implicitHeight: Theme.controlMinHeight
    AppTextField {
        anchors.left: parent.left
        anchors.right: calendarButton.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        inputMask: "9999-99-99"
        onEditingFinished: {
            const parts = text.split("-");
            const candidate = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]));
            if (Qt.formatDate(candidate, "yyyy-MM-dd") === text)
                root.selectedDate = candidate;
            text = root.text;
        }
    }
    IconButton {
        id: calendarButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        iconSource: Qt.resolvedUrl("icons/chevron-down.svg")
        iconWidth: 12
        iconHeight: 12
        tooltip: qsTrId("net.ui.choose_date")
        active: calendar.visible
        onClicked: {
            if (calendar.visible) {
                calendar.close();
            } else {
                calendar.monthDate = root.selectedDate;
                calendar.open();
            }
        }
    }
    Popup {
        id: calendar
        parent: root
        popupType: Popup.Item
        margins: 12
        padding: 12
        property date monthDate: root.selectedDate
        readonly property Item viewport: Overlay.overlay
        readonly property var calendarLocale: Qt.locale()
        readonly property real cellSize: {
            let labelWidth = calendarMetrics.advanceWidth("31");
            for (let day = 0; day < 7; ++day)
                labelWidth = Math.max(labelWidth, calendarMetrics.advanceWidth(calendarLocale.dayName(day, Locale.ShortFormat)));
            return Math.ceil(Math.max(Theme.controlMinHeight, labelWidth + 16, calendarMetrics.height + 12));
        }
        readonly property real gridSpacing: 4
        implicitWidth: Math.max(monthHeader.implicitWidth, 7 * cellSize + 6 * gridSpacing) + leftPadding + rightPadding
        implicitHeight: calendarContent.implicitHeight + topPadding + bottomPadding
        width: viewport ? Math.min(implicitWidth, Math.max(0, viewport.width - 2 * margins)) : implicitWidth
        height: viewport ? Math.min(implicitHeight, Math.max(0, viewport.height - 2 * margins)) : implicitHeight

        function reposition() {
            if (!viewport)
                return;
            const anchor = root.mapToItem(viewport, 0, 0);
            const below = anchor.y + root.height + 4;
            const above = anchor.y - height - 4;
            const preferredY = below + height <= viewport.height - margins ? below : above;
            x = Math.max(margins, Math.min(anchor.x, viewport.width - width - margins)) - anchor.x;
            y = Math.max(margins, Math.min(preferredY, viewport.height - height - margins)) - anchor.y;
        }

        onAboutToShow: reposition()
        onWidthChanged: if (visible)
            reposition()
        onHeightChanged: if (visible)
            reposition()
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        background: Rectangle {
            color: Theme.colors.background.panel
            border.color: Theme.colors.border.control
            radius: 8
        }
        FontMetrics {
            id: calendarMetrics
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            font.bold: true
        }
        Connections {
            target: calendar.viewport
            function onWidthChanged() {
                if (calendar.visible)
                    calendar.reposition();
            }
            function onHeightChanged() {
                if (calendar.visible)
                    calendar.reposition();
            }
        }
        Connections {
            target: root
            function onXChanged() {
                if (calendar.visible)
                    calendar.reposition();
            }
            function onYChanged() {
                if (calendar.visible)
                    calendar.reposition();
            }
            function onWidthChanged() {
                if (calendar.visible)
                    calendar.reposition();
            }
            function onHeightChanged() {
                if (calendar.visible)
                    calendar.reposition();
            }
        }
        contentItem: Flickable {
            clip: true
            implicitHeight: calendarContent.implicitHeight
            contentWidth: width
            contentHeight: calendarContent.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: calendarContent
                width: parent.width
                spacing: 8
                RowLayout {
                    id: monthHeader
                    Layout.fillWidth: true
                    IconButton {
                        glyph: "‹"
                        tooltip: qsTrId("net.ui.previous_month")
                        onClicked: calendar.monthDate = new Date(calendar.monthDate.getFullYear(), calendar.monthDate.getMonth() - 1, 1)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: Qt.formatDate(calendar.monthDate, "yyyy-MM")
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        color: Theme.colors.text.active
                        horizontalAlignment: Text.AlignHCenter
                    }
                    IconButton {
                        glyph: "›"
                        tooltip: qsTrId("net.ui.next_month")
                        onClicked: calendar.monthDate = new Date(calendar.monthDate.getFullYear(), calendar.monthDate.getMonth() + 1, 1)
                    }
                }
                DayOfWeekRow {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    implicitWidth: 7 * calendar.cellSize + 6 * spacing
                    implicitHeight: calendarMetrics.height + topPadding + bottomPadding
                    spacing: calendar.gridSpacing
                    locale: calendar.calendarLocale
                    delegate: Label {
                        required property string shortName
                        text: shortName
                        font: calendarMetrics.font
                        color: Theme.colors.text.secondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                MonthGrid {
                    id: monthGrid
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    implicitWidth: 7 * calendar.cellSize + 6 * spacing
                    implicitHeight: 6 * calendar.cellSize + 5 * spacing
                    Layout.preferredHeight: implicitHeight
                    spacing: calendar.gridSpacing
                    month: calendar.monthDate.getMonth()
                    year: calendar.monthDate.getFullYear()
                    locale: calendar.calendarLocale
                    delegate: Rectangle {
                        required property var model
                        color: Qt.formatDate(model.date, "yyyy-MM-dd") === root.text ? Theme.colors.accent.primary : "transparent"
                        radius: 4
                        Label {
                            anchors.centerIn: parent
                            text: parent.model.day
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.uiFontSize
                            color: parent.model.month === monthGrid.month ? Theme.colors.text.active : Theme.colors.text.disabled
                        }
                    }
                    onClicked: date => {
                        root.selectedDate = date;
                        calendar.close();
                    }
                }
            }
        }
    }
}
