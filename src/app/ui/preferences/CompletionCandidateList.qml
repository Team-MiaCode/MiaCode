pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Column {
    id: root
    required property var candidateModel
    required property string title
    required property string inputHint
    required property Flickable scrollViewport
    signal ensureVisibleRequested(Item item)
    signal scrollRequested(real distance)
    property string draggingKey: ""
    property Item draggedItem: null
    property Item focusedInput: null
    property real draggedY: 0
    property real pointerY: 0
    property real pointerOffsetY: 0
    property int sourceRow: -1
    property int dropRow: -1
    readonly property real rowHeight: Theme.controlMinHeight + 10
    readonly property real rowStride: root.rowHeight + rows.spacing
    readonly property color accentColor: Theme.colors.accent.primary
    readonly property real scrollVelocity: {
        if (root.draggingKey.length === 0 || !root.scrollViewport) return 0
        let velocity = 0
        if (root.pointerY < 28)
            velocity = -240 * Math.min(1, (28 - root.pointerY) / 28)
        else if (root.pointerY > root.scrollViewport.height - 28)
            velocity = 240 * Math.min(1, (root.pointerY - root.scrollViewport.height + 28) / 28)
        const minimum = root.scrollViewport.originY
        const maximum = minimum + Math.max(0,
            root.scrollViewport.contentHeight - root.scrollViewport.height)
        if ((velocity < 0 && root.scrollViewport.contentY <= minimum)
            || (velocity > 0 && root.scrollViewport.contentY >= maximum)) return 0
        return velocity
    }
    spacing: 12

    function ensureContentLayout() {
        rows.ensurePolished()
        root.ensurePolished()
    }
    function releaseInputFocus() {
        if (root.focusedInput) root.focusedInput.focus = false
    }
    function releaseInputFocusOutside(origin, position) {
        const input = root.focusedInput
        if (input && !input.contains(input.mapFromItem(origin, position)))
            root.releaseInputFocus()
    }
    TapHandler {
        parent: root
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        gesturePolicy: TapHandler.DragThreshold
        onPressedChanged: {
            if (pressed) root.releaseInputFocusOutside(root, point.position)
        }
    }

    function cancelDrag() {
        root.draggingKey = ""
        root.draggedItem = null
        root.sourceRow = -1
        root.dropRow = -1
    }
    function updateDragPosition() {
        if (root.draggingKey.length === 0) return
        const position = root.scrollViewport
            ? root.scrollViewport.mapToItem(rows, 0, root.pointerY).y : root.pointerY
        root.draggedY = position - root.pointerOffsetY
        root.dropRow = Math.max(0, Math.min(cards.count - 2,
            Math.round(root.draggedY / root.rowStride)))
    }
    function updatePointer(area, position) {
        root.pointerY = area.mapToItem(root.scrollViewport ? root.scrollViewport : rows, position).y
        root.updateDragPosition()
    }
    onVisibleChanged: {
        if (!visible) {
            root.releaseInputFocus()
            root.cancelDrag()
        }
    }

    Timer {
        interval: 16
        repeat: true
        running: root.draggingKey.length > 0 && root.scrollVelocity !== 0
        onTriggered: root.scrollRequested(root.scrollVelocity * interval / 1000)
    }
    Connections {
        target: root.scrollViewport
        function onContentYChanged() { root.updateDragPosition() }
        function onMovingChanged() {
            if (root.scrollViewport.moving) root.releaseInputFocus()
        }
    }

    Text {
        text: root.title
        color: Theme.colors.text.section
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize + 1
        font.weight: Theme.sectionTitleFontWeight
    }

    Item {
        width: parent.width
        height: rows.implicitHeight

        Rectangle {
            width: rows.width
            height: root.rowHeight
            y: root.dropRow * root.rowStride
            visible: root.draggingKey.length > 0 && root.dropRow >= 0
            radius: Theme.controlRadius
            color: Theme.surfaceColor(Qt.tint(Theme.colors.background.surface,
                Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.07)))
            border.width: Theme.controlBorderWidth
            border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.17)
            Behavior on y {
                enabled: root.draggingKey.length > 0
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }

        Column {
            id: rows
            width: parent.width
            spacing: 6
            z: 1

            Repeater {
                id: cards
                model: root.candidateModel
                delegate: Rectangle {
                    id: card
                    required property int index
                    required property string candidateKey
                    required property string draftValue
                    required property bool placeholderRow
                    required property bool removable
                    required property string validationMessage
                    readonly property real previewOffset: {
                        if (root.draggingKey.length === 0 || card.placeholderRow
                            || card.candidateKey === root.draggingKey) return 0
                        if (root.dropRow > root.sourceRow && card.index > root.sourceRow
                            && card.index <= root.dropRow) return -root.rowStride
                        if (root.dropRow < root.sourceRow && card.index >= root.dropRow
                            && card.index < root.sourceRow) return root.rowStride
                        return 0
                    }
                    width: rows.width
                    height: root.rowHeight
                    radius: Theme.controlRadius
                    color: {
                        const panel = Qt.color(Theme.colors.background.panel)
                        return Theme.surfaceColor(Qt.tint(Theme.colors.background.surface,
                            Qt.rgba(panel.r, panel.g, panel.b, 0.62)))
                    }
                    opacity: card.placeholderRow ? 0.75 : 1
                    transform: Translate {
                        y: card.previewOffset
                        Behavior on y {
                            enabled: root.draggingKey.length > 0
                            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                        }
                    }

                    Item {
                        id: grabArea
                        x: 5
                        y: 4
                        width: Math.max(0, card.width - 34)
                        height: card.height - 8
                        visible: !card.placeholderRow
                        HoverHandler {
                            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        }
                        DragHandler {
                            id: drag
                            target: null
                            acceptedButtons: Qt.LeftButton
                            enabled: !card.placeholderRow
                            onActiveChanged: {
                                if (active) {
                                    root.releaseInputFocus()
                                    root.sourceRow = card.index
                                    root.dropRow = card.index
                                    root.draggedItem = card
                                    root.draggedY = card.y
                                    root.pointerOffsetY = grabArea.mapToItem(card, centroid.position).y
                                    root.updatePointer(grabArea, centroid.position)
                                    root.draggingKey = card.candidateKey
                                    root.updateDragPosition()
                                } else if (root.draggingKey === card.candidateKey) {
                                    const key = root.draggingKey
                                    const destination = root.dropRow
                                    root.cancelDrag()
                                    root.candidateModel.moveCandidate(key, destination)
                                }
                            }
                            onTranslationChanged: {
                                if (active && root.draggingKey === card.candidateKey)
                                    root.updatePointer(grabArea, centroid.position)
                            }
                            onCanceled: root.cancelDrag()
                        }
                    }

                    AppTextField {
                        id: valueInput
                        x: 18
                        anchors.verticalCenter: parent.verticalCenter
                        height: Math.max(28, Theme.controlMinHeight - 2)
                        width: Math.max(0, Math.min(card.width * 0.75, card.width - 62,
                            Math.max(card.width * 0.375, valueMetrics.advanceWidth + leftPadding + rightPadding + 6)))
                        leftPadding: 4
                        rightPadding: 4
                        topPadding: 3
                        bottomPadding: 3
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        color: Theme.colors.text.primary
                        placeholderTextColor: Theme.colors.text.primary
                        placeholderText: card.placeholderRow ? root.inputHint : ""
                        text: card.draftValue
                        background: Item {}
                        property int pendingCursorPosition: -1
                        Accessible.name: root.title
                        Accessible.description: card.validationMessage
                        onTextEdited: {
                            const normalized = root.candidateModel.normalizeInput(text)
                            if (normalized !== text) {
                                pendingCursorPosition = root.candidateModel.normalizeInput(
                                    text.slice(0, cursorPosition)).length
                            }
                            root.candidateModel.setDraft(card.candidateKey, text)
                        }
                        onTextChanged: {
                            if (pendingCursorPosition >= 0) {
                                const position = pendingCursorPosition
                                pendingCursorPosition = -1
                                cursorPosition = position
                            }
                        }
                        onAccepted: root.candidateModel.commit(card.candidateKey)
                        onActiveFocusChanged: {
                            if (activeFocus) {
                                root.focusedInput = valueInput
                                root.ensureVisibleRequested(card)
                            } else {
                                if (root.focusedInput === valueInput) root.focusedInput = null
                                const model = root.candidateModel
                                const key = card.candidateKey
                                Qt.callLater(function() { model.commit(key) })
                            }
                        }
                        TextMetrics {
                            id: valueMetrics
                            font: valueInput.font
                            text: valueInput.text.length > 0 ? valueInput.text : valueInput.placeholderText
                        }
                        Tooltip {
                            visible: valueInput.hovered && text.length > 0
                            text: card.validationMessage.length > 0 ? card.validationMessage
                                : valueMetrics.advanceWidth > valueInput.width - valueInput.leftPadding - valueInput.rightPadding
                                    ? valueInput.text : ""
                        }
                    }

                    AbstractButton {
                        id: removeButton
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        visible: !card.placeholderRow
                        enabled: card.removable
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        Accessible.name: qsTrId("preferences.completion.delete")
                        onPressed: root.candidateModel.prepareRemoval(card.candidateKey)
                        onCanceled: root.candidateModel.cancelRemoval()
                        onClicked: root.candidateModel.removeCandidate(card.candidateKey)
                        contentItem: Item {
                            ControlsImpl.IconImage {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                source: Qt.resolvedUrl("icons/close.svg")
                                sourceSize: Theme.iconSourceSize(14, 14)
                                color: card.removable ? Theme.colors.text.secondary : Theme.colors.text.disabled
                            }
                        }
                        background: Rectangle {
                            radius: Theme.compactControlRadius
                            color: removeButton.down ? Theme.colors.buttonState.pressed
                                : removeButton.hovered && removeButton.enabled
                                    ? Theme.colors.buttonState.hover : "transparent"
                        }
                    }
                }
            }
        }
        ShaderEffectSource {
            visible: root.draggingKey.length > 0 && root.draggedItem !== null
            sourceItem: root.draggedItem
            hideSource: visible
            live: visible
            width: root.draggedItem ? root.draggedItem.width : 0
            height: root.rowHeight
            x: 0
            y: root.draggedY
            smooth: true
            z: 2
        }
    }
    Connections {
        target: root.candidateModel
        function onModelAboutToBeReset() { root.cancelDrag() }
        function onRowsAboutToBeRemoved() { root.cancelDrag() }
        function onRowsAboutToBeMoved() { root.cancelDrag() }
    }
}
