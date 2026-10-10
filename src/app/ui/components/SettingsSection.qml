import QtQuick
import QtQuick.Layouts
import MiaCode.UI

// Section header: caption and optional badge followed by a divider.
ColumnLayout {
    id: root

    required property string title
    property bool first: false
    // Optional count shown after the title, e.g. how many items a list holds.
    property string badge: ""
    default property alias content: contentColumn.data

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: root.first ? 0 : 8
        spacing: Theme.settingsRowSpacing

        Text {
            // An empty title leaves just the rule, for a group that needs
            // separating but no caption.
            visible: root.title.length > 0
            text: root.title
            color: Theme.colors.text.section
            font.family: Theme.uiFont
            font.pixelSize: Theme.sectionTitleFontSize
            font.weight: Theme.sectionTitleFontWeight
        }
        Rectangle {
            visible: root.badge.length > 0
            implicitWidth: Math.max(implicitHeight, badgeText.implicitWidth + 12)
            implicitHeight: 18
            radius: height / 2
            color: Theme.colors.popupState.selected

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: root.badge
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.secondaryFontSize
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: 1
            color: Theme.colors.border.normal
        }
    }

    ColumnLayout {
        id: contentColumn
        Layout.fillWidth: true
        spacing: Theme.settingsRowSpacing
    }
}
