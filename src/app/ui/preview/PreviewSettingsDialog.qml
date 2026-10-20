import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

AppDialog {
    id: root

    required property var previewSettings

    property int currentPageIndex: 0
    readonly property real sidebarWidth: root.width < 650 ? 140 : 180
    readonly property int settingsRowHeight: Theme.controlMinHeight + 10
    readonly property int sectionSpacing: 2 * Theme.settingsRowSpacing
    readonly property var pageTitles: root.previewSettings ? [root.previewSettings.videoGroupLabel, root.previewSettings.gameplayGroupLabel, root.previewSettings.skinGroupLabel] : []

    title: qsTrId("action.video_settings")
    preferredWidth: 960
    preferredHeight: Theme.dialogHeight + 140
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    header: null
    footer: null

    component NavigationButton: ChromeRow {
        id: navigationButton
        required property int pageIndex
        Layout.fillWidth: true
        Layout.minimumHeight: root.settingsRowHeight
        Layout.maximumHeight: root.settingsRowHeight
        implicitHeight: root.settingsRowHeight
        selected: root.currentPageIndex === navigationButton.pageIndex
        text: root.pageTitles[navigationButton.pageIndex] || ""
        labelFont: Qt.font({
            family: Theme.uiFont,
            pixelSize: Theme.uiFontSize,
            weight: navigationButton.selected ? Font.DemiBold : Font.Normal
        })
        Accessible.role: Accessible.PageTab
        Accessible.name: navigationButton.text
        Accessible.selected: navigationButton.selected
        onClicked: root.currentPageIndex = navigationButton.pageIndex
        Keys.onUpPressed: root.currentPageIndex = Math.max(0, navigationButton.pageIndex - 1)
        Keys.onDownPressed: root.currentPageIndex = Math.min(root.pageTitles.length - 1, navigationButton.pageIndex + 1)
    }

    function openPage(pageId) {
        root.currentPageIndex = pageId === "gameplay" ? 1 : pageId === "skin" ? 2 : 0;
        root.open();
    }

    onOpened: if (root.previewSettings)
        root.previewSettings.refresh()

    background: FloatingCard {
        id: dialogSurface
        popup: root
        tintColor: Theme.dialogTintColor
        blurRadius: Theme.dialogBlurRadius
        shadowBlur: 0.68
        shadowOpacity: Theme.dialogShadowOpacity
        shadowVerticalOffset: 3
        tintDelegate: Item {
            readonly property real tintOpacity: dialogSurface.materialActive ? Theme.dialogTintOpacity : 1
            readonly property color sidebarTint: Theme.colors.background.surface
            readonly property color panelTint: Theme.colors.background.panel

            Rectangle {
                width: root.sidebarWidth
                height: parent.height
                topLeftRadius: Theme.popupRadius
                bottomLeftRadius: Theme.popupRadius
                color: Qt.rgba(parent.sidebarTint.r, parent.sidebarTint.g, parent.sidebarTint.b, parent.tintOpacity)
            }
            Rectangle {
                x: root.sidebarWidth
                width: parent.width - x
                height: parent.height
                topRightRadius: Theme.popupRadius
                bottomRightRadius: Theme.popupRadius
                color: Qt.rgba(parent.panelTint.r, parent.panelTint.g, parent.panelTint.b, parent.tintOpacity)
            }
        }
    }

    contentItem: RowLayout {
        id: settingsBody
        spacing: 0

        Item {
            Layout.preferredWidth: root.sidebarWidth
            Layout.fillHeight: true

            Rectangle {
                anchors.right: parent.right
                width: 1 / Screen.devicePixelRatio
                height: parent.height
                color: Theme.separatorColor
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 24
                anchors.bottomMargin: 24
                spacing: 4

                Repeater {
                    model: root.pageTitles.length
                    delegate: NavigationButton {
                        required property int index
                        pageIndex: index
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root.width < 650 ? 18 : 32
            Layout.rightMargin: root.width < 650 ? 18 : 32
            Layout.topMargin: 24
            Layout.bottomMargin: 24
            spacing: root.sectionSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.settingsRowSpacing

                Text {
                    Layout.fillWidth: true
                    text: root.pageTitles[root.currentPageIndex] || root.title
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize + 8
                    font.weight: Font.DemiBold
                    color: Theme.colors.text.heading
                    elide: Text.ElideRight
                }
                IconButton {
                    objectName: "previewSettingsCloseButton"
                    iconSource: Qt.resolvedUrl("icons/close.svg")
                    tooltip: qsTrId("action.close")
                    onClicked: root.reject()
                }
            }

            ScrollView {
                id: pageViewport
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical: AppScrollBar {}

                PreviewAppearancePages {
                    width: pageViewport.availableWidth
                    previewSettings: root.previewSettings
                    currentIndex: root.currentPageIndex
                }
            }
        }
    }
}
