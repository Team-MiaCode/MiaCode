import QtQuick
import QtQuick.Controls
import MiaCode.UI

// Completion candidates for the QML source editor. Focus stays on the editor —
// SourceEditor routes ↑ ↓ Tab Enter Esc through the controller — so this popup
// is a pure projection of the controller session and never grabs input. Its job
// is to show *which* candidate those keys are acting on.
AppDropdownPanel {
    id: root

    required property var editor
    required property var controller
    readonly property bool pointerInside: popupHover.hovered
    signal settingsRequested(string group)
    readonly property bool customizable: root.controller.completionGroup.length > 0
    readonly property real settingsSlotWidth: customizable ? Theme.controlMinHeight + 3 : 0

    parent: Overlay.overlay
    modal: false
    focus: false
    closePolicy: Popup.NoAutoClose
    visible: editor.navigationVisible && controller.completionActive && controller.completionCandidates.length > 0

    readonly property real maximumHeight: root.parent
        ? Math.max(0, Math.min(260, root.parent.height - 2 * root.margins)) : 260
    property real candidatesWidth: 0

    // A vertical ListView never computes contentWidth, so the previous
    // `implicitWidth: contentWidth` binding resolved to -1 and pinned the popup
    // at its minimum width no matter how wide the candidates were.
    function recomputeCandidateWidth() {
        const candidates = root.controller.completionCandidates
        let widest = 0
        for (let i = 0; i < candidates.length; ++i) {
            candidateMetrics.text = candidates[i]
            widest = Math.max(widest, candidateMetrics.advanceWidth)
        }
        root.candidatesWidth = widest
    }

    width: Math.min(root.parent ? Math.max(0, root.parent.width - 2 * root.margins) : Infinity,
                    Math.max(120, Math.ceil(root.candidatesWidth)
                        + root.leftPadding + root.rightPadding + 24 + root.settingsSlotWidth))
    height: Math.min(root.maximumHeight,
                     Math.max(Theme.controlMinHeight, candidateList.contentHeight)
                         + root.topPadding + root.bottomPadding)

    function updateAnchor() {
        if (!root.visible || !root.parent || !root.editor)
            return
        const caret = root.editor.cursorRectangle
        const below = root.editor.mapToItem(root.parent, caret.x, caret.y + caret.height)
        const above = root.editor.mapToItem(root.parent, caret.x, caret.y)
        root.x = Math.max(0, Math.min(below.x, Math.max(0, root.parent.width - root.width)))
        // Flip above the caret rather than hang off the bottom of the overlay.
        root.y = (below.y + root.height <= root.parent.height) || (above.y - root.height < 0)
            ? below.y
            : above.y - root.height
    }

    onWidthChanged: root.updateAnchor()
    onHeightChanged: root.updateAnchor()
    onVisibleChanged: {
        if (root.visible) {
            root.recomputeCandidateWidth()
            root.updateAnchor()
        }
    }

    Connections {
        target: root.editor
        enabled: root.visible
        function onCursorRectangleChanged() { root.updateAnchor() }
        function onLayoutChanged() {
            root.recomputeCandidateWidth()
            root.updateAnchor()
            candidateList.positionViewAtIndex(root.controller.completionIndex, ListView.Contain)
        }
        function onScenePositionChanged() { root.updateAnchor() }
    }

    Connections {
        target: root.controller
        function onCompletionChanged() {
            root.recomputeCandidateWidth()
            // Keyboard navigation must stay visible even when the session has
            // more candidates than the popup can show at once.
            candidateList.positionViewAtIndex(root.controller.completionIndex, ListView.Contain)
        }
    }

    Connections {
        target: root.parent
        enabled: root.visible
        function onWidthChanged() { root.updateAnchor() }
        function onHeightChanged() { root.updateAnchor() }
    }

    contentItem: Item {
        Item {
            x: -root.leftPadding
            y: -root.topPadding
            width: parent.width + root.leftPadding + root.rightPadding
            height: parent.height + root.topPadding + root.bottomPadding
            HoverHandler { id: popupHover }
        }
        TextMetrics {
            id: candidateMetrics
            font: root.editor.effectiveFont
        }
        ListView {
            id: candidateList
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: settingsArea.left
            model: root.controller.completionCandidates
            currentIndex: root.controller.completionIndex
            clip: true
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}

            delegate: ChromeRow {
                id: candidateRow
                required property string modelData
                required property int index
                stateColors: Theme.colors.popupState
                width: candidateList.width
                implicitHeight: Math.max(Theme.controlMinHeight, root.editor.lineHeight,
                    implicitContentHeight + 2 * (Theme.chromePadding + Theme.chromeInsetY))
                highlighted: candidateRow.index === root.controller.completionIndex
                text: candidateRow.modelData
                labelFont: root.editor.effectiveFont
                onClicked: {
                    root.controller.selectCompletionIndex(candidateRow.index)
                    root.editor.acceptCompletionFromPopup()
                }
            }
        }
        Item {
            id: settingsArea
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.settingsSlotWidth
            IconButton {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: root.customizable
                width: Theme.controlMinHeight
                height: Theme.controlMinHeight
                iconWidth: 20
                iconHeight: 20
                focusPolicy: Qt.NoFocus
                iconSource: Qt.resolvedUrl("icons/settings.svg")
                tooltip: qsTrId("preferences.completion.customize")
                onClicked: {
                    const group = root.controller.completionGroup
                    root.controller.closeCompletion()
                    root.settingsRequested(group)
                }
            }
        }
    }
}
