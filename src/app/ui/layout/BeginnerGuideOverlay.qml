pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import MiaCode.UI

Popup {
    id: root

    required property var sections
    property int selectedIndex: 0
    readonly property var selectedSection: sections[selectedIndex]
    readonly property rect spotlight: regionRect(selectedSection)
    readonly property bool usingFallback: !itemAvailable(selectedSection.target)
    readonly property bool lastStep: selectedIndex === sections.length - 1
    readonly property int iconSize: Theme.compactControlHeight

    parent: Overlay.overlay
    popupType: Popup.Item
    width: parent.width
    height: parent.height
    padding: 0
    margins: 0
    modal: true
    dim: false
    focus: true
    closePolicy: Popup.CloseOnEscape
    z: 100
    background: Item {}
    onAboutToShow: selectedIndex = 0
    onOpened: nextButton.forceActiveFocus()
    onSelectedIndexChanged: viewport.contentItem.contentY = 0

    function itemAvailable(item) {
        if (!item || item.Window.window !== root.parent.Window.window)
            return false
        for (let node = item; node; node = node.parent) {
            if (!node.visible || node.width <= 0 || node.height <= 0)
                return false
        }
        return true
    }

    function regionRect(section) {
        if (!root.visible || !section)
            return Qt.rect(0, 0, 0, 0)
        if (!itemAvailable(section.target))
            return itemRect(section.fallbackTarget, 0)
        const primary = itemRect(section.target, section.focusHeight || 0)
        if (!itemAvailable(section.secondaryTarget))
            return primary
        const secondary = itemRect(section.secondaryTarget, 0)
        const left = Math.min(primary.x, secondary.x)
        const top = Math.min(primary.y, secondary.y)
        const right = Math.max(primary.x + primary.width, secondary.x + secondary.width)
        const bottom = Math.max(primary.y + primary.height, secondary.y + secondary.height)
        return Qt.rect(left, top, right - left, bottom - top)
    }

    function itemRect(item, heightLimit) {
        if (!itemAvailable(item))
            return Qt.rect(0, 0, 0, 0)
        // 读取祖先几何，使高亮跟随分栏、滚动和窗口缩放。
        for (let node = item; node; node = node.parent) {
            node.x
            node.y
        }
        const origin = item.mapToItem(surface, 0, 0)
        const focusHeight = heightLimit > 0 ? Math.min(item.height, heightLimit) : item.height
        const inset = Theme.chromePadding
        let left = Math.max(0, origin.x - inset)
        let top = Math.max(0, origin.y - inset)
        let right = Math.min(surface.width, origin.x + item.width + inset)
        let bottom = Math.min(surface.height, origin.y + focusHeight + inset)
        for (let node = item.parent; node; node = node.parent) {
            if (node.clip) {
                const clipOrigin = node.mapToItem(surface, 0, 0)
                left = Math.max(left, clipOrigin.x)
                top = Math.max(top, clipOrigin.y)
                right = Math.min(right, clipOrigin.x + node.width)
                bottom = Math.min(bottom, clipOrigin.y + node.height)
            }
        }
        return Qt.rect(left, top, Math.max(0, right - left), Math.max(0, bottom - top))
    }

    function bounded(value, minimum, maximum) {
        return Math.max(minimum, Math.min(value, maximum))
    }

    function placeCard(cardWidth, desiredHeight) {
        const margin = Theme.dialogMargin
        const gap = Theme.dialogPadding
        const region = root.spotlight
        const availableHeight = surface.height - 2 * margin
        const cardHeight = Math.min(desiredHeight, availableHeight)
        const preferred = root.usingFallback ? "below" : root.selectedSection.placement
        const alignEnd = root.usingFallback || root.selectedSection.alignment === "end"
        const spaces = {
            below: surface.height - margin - region.y - region.height - gap,
            above: region.y - gap - margin,
            right: surface.width - margin - region.x - region.width - gap,
            left: region.x - gap - margin
        }
        const sides = [preferred, "below", "right", "above", "left"]
        let side = ""
        for (const candidate of sides) {
            const requiredSpace = candidate === "below" || candidate === "above"
                ? cardHeight : cardWidth
            if (spaces[candidate] >= requiredSpace) {
                side = candidate
                break
            }
        }
        // 短窗口使用上下空隙，说明正文滚动，按钮保留在卡片底部。
        if (side === "")
            side = spaces.below >= spaces.above ? "below" : "above"
        const height = side === "below" || side === "above"
            ? Math.min(cardHeight, Math.max(0, spaces[side])) : cardHeight
        let x = alignEnd ? region.x + region.width - cardWidth : region.x
        let y = region.y
        if (side === "below")
            y = region.y + region.height + gap
        else if (side === "above")
            y = region.y - gap - height
        else if (side === "right")
            x = region.x + region.width + gap
        else
            x = region.x - gap - cardWidth
        return {
            x: bounded(x, margin, surface.width - margin - cardWidth),
            y: bounded(y, margin, surface.height - margin - height),
            height: height,
            side: side
        }
    }

    function advance() {
        if (lastStep)
            close()
        else
            selectedIndex += 1
    }

    contentItem: Item {
        id: surface
        Accessible.name: qsTrId("guide.page_one")

        Rectangle {
            width: parent.width
            height: root.spotlight.y
            color: Theme.modalScrimColor
        }
        Rectangle {
            y: root.spotlight.y
            width: root.spotlight.x
            height: root.spotlight.height
            color: Theme.modalScrimColor
        }
        Rectangle {
            x: root.spotlight.x + root.spotlight.width
            y: root.spotlight.y
            width: parent.width - x
            height: root.spotlight.height
            color: Theme.modalScrimColor
        }
        Rectangle {
            y: root.spotlight.y + root.spotlight.height
            width: parent.width
            height: parent.height - y
            color: Theme.modalScrimColor
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }

        Rectangle {
            x: root.spotlight.x
            y: root.spotlight.y
            width: root.spotlight.width
            height: root.spotlight.height
            visible: width > 0 && height > 0
            color: "transparent"
            radius: Theme.controlRadius
            border.width: 2
            border.color: Theme.colors.accent.primary
        }

        Item {
            id: card
            readonly property real padding: Theme.dialogPadding
            readonly property real desiredHeight: header.implicitHeight
                + bodyColumn.implicitHeight + footer.implicitHeight
                + 2 * padding + 2 * Theme.dialogPadding
            readonly property var placement: root.placeCard(width, desiredHeight)
            width: Math.min(320, surface.width - 2 * Theme.dialogMargin)
            height: placement.height
            x: placement.x
            y: placement.y

            // 指示角位于卡片边缘，序号放在卡片内，控件图标保持可见。
            Rectangle {
                readonly property bool verticalSide: card.placement.side === "below"
                    || card.placement.side === "above"
                width: Theme.dialogPadding
                height: width
                rotation: 45
                x: verticalSide
                    ? root.bounded(root.spotlight.x + root.spotlight.width / 2 - card.x,
                        2 * card.padding, card.width - 2 * card.padding) - width / 2
                    : card.placement.side === "right" ? -width / 2 : card.width - width / 2
                y: verticalSide
                    ? card.placement.side === "below" ? -height / 2 : card.height - height / 2
                    : root.bounded(root.spotlight.y + Theme.controlMinHeight / 2 - card.y,
                        2 * card.padding, card.height - 2 * card.padding) - height / 2
                color: Theme.colors.background.elevated
                border.color: Theme.floatingBorderColor
                border.width: 1
                visible: root.spotlight.width > 0 && root.spotlight.height > 0
            }

            FloatingCard {
                anchors.fill: parent
                cornerRadius: Theme.popupRadius
                tintColor: Theme.colors.background.elevated
                shadowOpacity: Theme.dialogShadowOpacity
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: card.padding
                spacing: Theme.dialogPadding

                ColumnLayout {
                    id: header
                    Layout.fillWidth: true
                    spacing: Theme.panelPadding

                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            Layout.preferredWidth: Theme.controlMinHeight
                            Layout.preferredHeight: Theme.controlMinHeight
                            radius: width / 2
                            color: Theme.colors.accent.primary
                            Text {
                                anchors.centerIn: parent
                                text: root.selectedIndex + 1
                                color: Theme.colors.text.onAccent
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.selectedSection.title
                            color: Theme.colors.text.heading
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.headingFontSize
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }
                        IconButton {
                            iconSource: Qt.resolvedUrl("icons/close.svg")
                            tooltip: qsTrId("guide.exit")
                            Accessible.name: tooltip
                            onClicked: root.close()
                        }
                    }

                }

                ScrollView {
                    id: viewport
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: bodyColumn.implicitHeight
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical: AppScrollBar {}

                    Column {
                        id: bodyColumn
                        width: viewport.availableWidth
                        spacing: Theme.panelPadding
                        GridLayout {
                            width: parent.width
                            columns: root.selectedSection.columns || 2
                            uniformCellWidths: true
                            uniformCellHeights: true
                            columnSpacing: Theme.panelPadding
                            rowSpacing: Theme.panelPadding

                            Repeater {
                                model: root.selectedSection.features
                                delegate: Item {
                                    id: feature
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.columnSpan: modelData.span || 1
                                    implicitHeight: Math.max(Theme.activityButtonSize,
                                        featureRow.implicitHeight + 2 * Theme.panelPadding)
                                    Accessible.role: Accessible.StaticText
                                    Accessible.name: modelData.label

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: Theme.controlRadius
                                        color: Theme.colors.background.control
                                        border.color: Theme.colors.border.normal
                                    }
                                    RowLayout {
                                        id: featureRow
                                        anchors.fill: parent
                                        anchors.margins: Theme.panelPadding
                                        spacing: Theme.panelPadding

                                        Item {
                                            Layout.preferredWidth: sample.visible
                                                ? Math.max(root.iconSize, sample.implicitWidth) : root.iconSize
                                            Layout.preferredHeight: root.iconSize
                                            ControlsImpl.IconImage {
                                                anchors.centerIn: parent
                                                width: root.iconSize
                                                height: width
                                                sourceSize: Qt.size(width, height)
                                                source: feature.modelData.icon || ""
                                                color: Theme.colors.text.active
                                                visible: feature.modelData.icon !== undefined
                                            }
                                            Text {
                                                id: sample
                                                anchors.centerIn: parent
                                                visible: feature.modelData.sample !== undefined
                                                text: feature.modelData.sample || ""
                                                color: feature.modelData.kind === "code"
                                                    ? Theme.colors.syntax.duration : Theme.colors.text.active
                                                font.family: feature.modelData.kind === "code"
                                                    ? Theme.codeFont.family : Theme.uiFont
                                                font.pixelSize: Theme.uiFontSize
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: feature.modelData.label
                                            color: Theme.colors.text.primary
                                            font.family: Theme.uiFont
                                            font.pixelSize: Theme.secondaryFontSize
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }
                        }
                        Text {
                            width: parent.width
                            visible: root.usingFallback
                            text: root.usingFallback ? (root.selectedSection.hint || "") : ""
                            color: Theme.colors.text.secondary
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.secondaryFontSize
                            wrapMode: Text.Wrap
                        }
                    }
                }

                RowLayout {
                    id: footer
                    Layout.fillWidth: true
                    spacing: Theme.panelPadding
                    AppButton {
                        text: qsTrId("guide.previous")
                        enabled: root.selectedIndex > 0
                        onClicked: root.selectedIndex -= 1
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTrId("guide.step_counter")
                            .arg(root.selectedIndex + 1).arg(root.sections.length)
                        color: Theme.colors.text.secondary
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.secondaryFontSize
                        horizontalAlignment: Text.AlignHCenter
                    }
                    AppButton {
                        id: nextButton
                        text: root.lastStep ? qsTrId("guide.finish") : qsTrId("guide.next")
                        emphasized: true
                        onClicked: root.advance()
                    }
                }
            }
        }
    }
}
