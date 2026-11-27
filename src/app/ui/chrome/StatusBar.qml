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

    readonly property bool lightWallpaperSurface: Theme.backgroundActive && !Theme.darkTheme && Theme.surfaceOpacity < 0.65
    readonly property color statusTextColor: lightWallpaperSurface ? Theme.colors.text.primary : Theme.colors.text.status
    readonly property var badgeStateColors: (!Theme.darkTheme && !root.lightWallpaperSurface)
        ? ({ hover: Qt.rgba(1, 1, 1, 0.16), pressed: Qt.rgba(1, 1, 1, 0.26), selected: Qt.rgba(1, 1, 1, 0.20) })
        : Theme.colors.buttonState

    implicitHeight: 23
    color: Theme.surfaceColor(root.lightWallpaperSurface ? Theme.colors.background.panel : Theme.colors.background.statusBar)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1 / (Screen.devicePixelRatio * Theme.uiScale)
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

        AbstractButton {
            id: updateBadge
            objectName: "statusBarUpdateBadge"
            visible: root.updateAvailable
            text: qsTrId("status.update_available").arg(root.updateVersion)
            Layout.preferredWidth: implicitWidth
            Layout.fillHeight: true
            implicitWidth: implicitContentWidth + leftPadding + rightPadding
            leftPadding: Theme.chromePadding
            rightPadding: Theme.chromePadding
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.name: text
            onClicked: root.updateActivated()

            contentItem: StatusText { text: updateBadge.text; verticalAlignment: Text.AlignVCenter }
            background: HoverChrome {
                fillBounds: true
                stateColors: root.badgeStateColors
                hovered: updateBadge.hovered
                pressed: updateBadge.down
                focused: updateBadge.visualFocus
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
            visible: root.difficultyActive && text.length > 0
            Layout.preferredWidth: implicitWidth
            HoverHandler { id: beatHover; enabled: root.selectionBeatTooltip.length > 0 }
            Tooltip { visible: beatHover.hovered && root.selectionBeatTooltip.length > 0; text: root.selectionBeatTooltip }
        }
        StatusText {
            text: "simai"
            visible: root.difficultyActive
            Layout.preferredWidth: implicitWidth
        }
    }

    component StatusText: Text {
        color: enabled ? root.statusTextColor : Theme.colors.text.disabled
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
    }
}
