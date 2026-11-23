import QtQuick
import MiaCode.UI

Rectangle {
    id: root

    property string title
    property bool showMore: false
    property bool sidebarTitle: false
    default property alias trailing: trailingRow.data

    readonly property real titleInkCenter: {
        const sample = root.title.length > 0 ? root.title : "汉"
        const rect = titleMetrics.tightBoundingRect(sample)
        return titleMetrics.ascent + rect.y + rect.height / 2
    }

    implicitHeight: root.sidebarTitle
                    ? Math.max(Theme.workspaceHeaderHeight,
                               Math.ceil(titleLabel.y + titleLabel.implicitHeight + Theme.chromeInsetY),
                               trailingRow.implicitHeight)
                    : Theme.workspaceHeaderHeight
    color: "transparent"

    FontMetrics {
        id: titleMetrics
        font: titleLabel.font
    }

    Text {
        id: titleLabel

        anchors.left: parent.left
        anchors.leftMargin: 10
        y: root.sidebarTitle ? Theme.workspaceHeaderContentCenterY - root.titleInkCenter : 0
        anchors.verticalCenter: root.sidebarTitle ? undefined : parent.verticalCenter
        text: root.title
        color: root.sidebarTitle ? Theme.colors.text.heading : Theme.colors.text.primary
        font.family: Theme.uiFont
        font.pixelSize: root.sidebarTitle ? Theme.headingFontSize : Theme.uiFontSize
        font.weight: root.sidebarTitle ? Font.DemiBold : Font.Normal
        verticalAlignment: Text.AlignTop
    }

    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: (root.sidebarTitle ? Theme.workspaceHeaderContentCenterY : root.height / 2)
           - height / 2
        spacing: 5
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: (root.sidebarTitle ? Theme.workspaceHeaderContentCenterY : root.height / 2)
           - height / 2
        visible: root.showMore && trailingRow.children.length === 0
        text: "..."
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
    }
}
