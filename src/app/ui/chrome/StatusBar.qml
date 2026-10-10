import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Rectangle {
    id: root

    property string documentName: ""
    property int cursorLine: 1
    property int cursorColumn: 1
    property string selectionBeatText: ""
    property string selectionBeatTooltip: ""
    property bool metadataActive: false
    property bool difficultyActive: false
    property bool updateAvailable: false
    property string updateVersion: ""

    signal updateActivated()

    implicitHeight: 23
    color: Theme.surfaceColor(Theme.colors.background.statusBar)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1 / Screen.devicePixelRatio
        color: Theme.separatorColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 14

        StatusText {
            Layout.fillWidth: true
            text: root.documentName
            visible: text.length > 0
            elide: Text.ElideMiddle
        }
        Item { Layout.fillWidth: root.documentName.length === 0 }

        StatusText {
            objectName: "statusBarUpdateBadge"
            visible: root.updateAvailable
            text: qsTrId("status.update_available").arg(root.updateVersion)
            color: Theme.colors.text.active
            Layout.preferredWidth: implicitWidth

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.updateActivated()
            }
        }

        StatusText {
            text: qsTrId("dialog.unsaved_field_changes.field.metadata")
            visible: root.metadataActive
            Layout.preferredWidth: implicitWidth
        }
        StatusText {
            text: qsTrId("qml.line_1_column_2").arg(root.cursorLine).arg(root.cursorColumn)
            visible: root.difficultyActive
            Layout.preferredWidth: implicitWidth
        }
        StatusText {
            text: root.selectionBeatText
            tooltipText: root.selectionBeatTooltip
            visible: root.difficultyActive && text.length > 0
            Layout.preferredWidth: implicitWidth
        }
        StatusText {
            text: "simai"
            visible: root.difficultyActive
            Layout.preferredWidth: implicitWidth
        }
    }

    component StatusText: Text {
        id: statusText
        property string tooltipText: ""
        color: enabled ? Theme.colors.text.status : Theme.colors.text.disabled
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            visible: statusText.tooltipText.length > 0
            ToolTip.visible: containsMouse
            ToolTip.text: statusText.tooltipText
        }
    }
}
