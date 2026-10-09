import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

Item {
    id: root

    required property var exportSession
    required property var previewSession

    // The same lane also picks the fixed-length 片头 PV segment (fixedLength):
    // both handles and the body move the window together, a press inside the
    // window or on the playhead seeks the preview within it, and a press
    // outside recentres the window there. The caller binds the range, routes
    // edits through applyRange and maps window offsets to preview seconds.
    property string objectNamePrefix: "exportRange"
    property bool fixedLength: false
    property var applyRange: function(start, end) { exportSession.setExportRangeSeconds(start, end) }
    property var previewSecondFor: function(second) { return previewSecondForRangeSecond(second) }
    // fixedLength: where the playhead sits in the window (seconds from its
    // start, may run past its end), or < 0 to place it at playheadSeconds when
    // the preview is inside the content, and hide it otherwise.
    property real windowPlayheadSeconds: -1
    // Where the playhead lands in the window whenever the window moves.
    property real windowMoveOffsetSeconds: 0
    property var previewSecondForWindowOffset: function(offset) { return previewSecondFor(startSeconds + offset) }
    property real dragWindowOffsetSeconds: 0
    property real dragPressX: 0
    property bool dragMoved: false
    readonly property bool windowMoving: fixedLength && draggingTarget === "range" && dragMoved
    // fixedLength: the user picked a moment inside the window (click or playhead drag).
    signal windowSeeked()

    property string draggingTarget: ""
    property real dragStartSeconds: 0
    property real dragEndSeconds: 0
    property real dragPressSecond: 0
    property real dragPreviewSecond: 0
    property real hoverSecond: 0

    property real totalSeconds: Math.max(0, Number(exportSession.contentDurationSeconds) || 0)
    property real minimumRangeSeconds: Math.max(0, Number(exportSession.minimumExportRangeSeconds) || 0)
    property real requestedStartSeconds: Number(exportSession.exportStartSeconds) || 0
    property real requestedEndSeconds: Number(exportSession.exportEndSeconds) || 0
    readonly property real startSeconds: Math.max(0, Math.min(totalSeconds, requestedStartSeconds))
    readonly property real endSeconds: Math.max(startSeconds + minimumRangeSeconds,
                                                 Math.min(totalSeconds, requestedEndSeconds))
    readonly property real playheadSeconds: Math.max(0, Math.min(totalSeconds,
                                                                  Number(previewSession.positionSeconds) || 0))
    // The lane can land on fractional logical coordinates on a fractional-DPR
    // display. Keep narrow playback geometry on the device-pixel grid so its
    // apparent width does not change as the center moves between samples.
    readonly property real renderDpr: Math.max(1, Number(Screen.devicePixelRatio) || 1)
    readonly property real timestampBandHeight: 0
    // What the overlay reads out. Hovering asks "what is under my pointer";
    // dragging asks "where is the thing I am moving", and those are not the
    // same second — grabbing the middle of the range and pushing it left put
    // the readout wherever the grab happened to land, which is a point with no
    // meaning to anyone.
    readonly property real displaySecond: draggingTarget.length > 0 ? dragPreviewSecond
                                                                    : hoverSecond
    readonly property real timestampWidth: Math.max(1,
                                                     Math.ceil(Math.max(
                                                         timestampMetrics.advanceWidth,
                                                         durationTimestampMetrics.advanceWidth)))

    implicitHeight: timestampBandHeight + lane.height + Theme.captionFontSize + 6
    Layout.fillWidth: true

    function formatSecond(second) {
        const milliseconds = Math.max(0, Math.round(second * 1000))
        const minutes = Math.floor(milliseconds / 60000)
        const seconds = Math.floor(milliseconds / 1000) % 60
        const millis = milliseconds % 1000
        return String(minutes).padStart(2, "0") + ":"
            + String(seconds).padStart(2, "0") + "."
            + String(millis).padStart(3, "0")
    }

    function previewSecondForRangeSecond(second) {
        return second <= 0.000001 && exportSession.introEnabled && exportSession.fullRangeExport
            ? previewSession.lowerBoundSeconds : second
    }

    function windowLengthSeconds() {
        return Math.max(0.001, endSeconds - startSeconds)
    }

    function windowOffsetForX(x) {
        const width = Math.max(1, lane.visualEndX - lane.visualStartX)
        return Math.max(0, Math.min(windowLengthSeconds() - 0.001,
                                    (x - lane.visualStartX) / width * windowLengthSeconds()))
    }

    function moveWindowTo(nextStart) {
        const length = windowLengthSeconds()
        const bounded = Math.max(0, Math.min(totalSeconds - length, nextStart))
        root.applyRange(bounded, bounded + length)
        return bounded
    }

    function beginWindowGesture(target, x) {
        draggingTarget = target
        dragPressX = x
        dragPressSecond = lane.secondForX(x)
        dragMoved = false
        dragWindowOffsetSeconds = target === "playhead" ? windowOffsetForX(x) : windowMoveOffsetSeconds
        if (target === "jump") {
            // Recentre on the press, then keep dragging the window from there.
            moveWindowTo(dragPressSecond - windowLengthSeconds() * 0.5)
            draggingTarget = "range"
            dragMoved = true
        }
        dragStartSeconds = startSeconds
        dragPreviewSecond = draggingTarget === "playhead" ? startSeconds + dragWindowOffsetSeconds : startSeconds
        previewSession.beginScrub()
        if (dragMoved || draggingTarget === "playhead")
            previewSession.updateScrub(root.previewSecondForWindowOffset(dragWindowOffsetSeconds))
    }

    function updateWindowGesture(x) {
        if (draggingTarget === "playhead") {
            dragWindowOffsetSeconds = windowOffsetForX(x)
            dragPreviewSecond = startSeconds + dragWindowOffsetSeconds
        } else if (draggingTarget === "range") {
            if (!dragMoved && Math.abs(x - dragPressX) < 3)
                return
            dragMoved = true
            const second = lane.unclampedSecondForX(x)
            dragPreviewSecond = moveWindowTo(second <= 0 ? 0
                                             : second >= totalSeconds ? totalSeconds
                                             : dragStartSeconds + second - dragPressSecond)
        } else {
            return
        }
        previewSession.updateScrub(root.previewSecondForWindowOffset(dragWindowOffsetSeconds))
    }

    function endWindowGesture() {
        if (draggingTarget.length === 0)
            return
        // A click inside the window (no drag) seeks to that moment of it.
        const seeked = draggingTarget === "playhead" || (draggingTarget === "range" && !dragMoved)
        if (draggingTarget === "range" && !dragMoved)
            dragWindowOffsetSeconds = windowOffsetForX(dragPressX)
        previewSession.endScrub(root.previewSecondForWindowOffset(dragWindowOffsetSeconds))
        draggingTarget = ""
        dragMoved = false
        if (seeked)
            root.windowSeeked()
    }

    function nudgeWindow(deltaSeconds) {
        moveWindowTo(startSeconds + deltaSeconds)
        previewSession.positionSeconds = root.previewSecondForWindowOffset(windowMoveOffsetSeconds)
    }

    activeFocusOnTab: fixedLength
    Keys.onPressed: function(event) {
        if (!root.fixedLength)
            return
        const step = (event.modifiers & Qt.ShiftModifier) ? 1.0 : 0.1
        if (event.key === Qt.Key_Left) {
            root.nudgeWindow(-step)
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            root.nudgeWindow(step)
            event.accepted = true
        }
    }

    function beginDrag(target, second) {
        draggingTarget = target
        dragStartSeconds = startSeconds
        dragEndSeconds = endSeconds
        dragPressSecond = second
        // A body drag reads out its start: that is the edge the range is being
        // placed by, and it is the one the label can sit against while both
        // ends move together.
        dragPreviewSecond = target === "end" ? endSeconds : startSeconds
        previewSession.beginScrub()
    }

    // `second` is the pointer's unclamped lane second. The edge follows the
    // pointer by the grab offset, so a press beside the handle centre would
    // stop short of the lane end; a pointer at or past the end of the track
    // pins the edge to it.
    function updateDrag(second) {
        const atStart = second <= 0
        const atEnd = second >= totalSeconds
        const delta = second - dragPressSecond
        if (draggingTarget === "start") {
            const nextStart = Math.max(0, Math.min(dragEndSeconds - minimumRangeSeconds,
                                                    atStart ? 0 : dragStartSeconds + delta))
            root.applyRange(nextStart, dragEndSeconds)
            dragPreviewSecond = nextStart
        } else if (draggingTarget === "end") {
            const nextEnd = Math.max(dragStartSeconds + minimumRangeSeconds,
                                     Math.min(totalSeconds, atEnd ? totalSeconds : dragEndSeconds + delta))
            root.applyRange(dragStartSeconds, nextEnd)
            dragPreviewSecond = nextEnd
        } else if (draggingTarget === "range") {
            const boundedDelta = atStart ? -dragStartSeconds
                : atEnd ? totalSeconds - dragEndSeconds
                : Math.max(-dragStartSeconds, Math.min(totalSeconds - dragEndSeconds, delta))
            const nextStart = dragStartSeconds + boundedDelta
            const nextEnd = dragEndSeconds + boundedDelta
            root.applyRange(nextStart, nextEnd)
            dragPreviewSecond = nextStart
        } else {
            return
        }
        previewSession.updateScrub(root.previewSecondFor(dragPreviewSecond))
    }

    function endDrag() {
        if (draggingTarget.length === 0)
            return
        previewSession.endScrub(root.previewSecondFor(dragPreviewSecond))
        draggingTarget = ""
    }

    TextMetrics {
        id: timestampMetrics

        font.family: Theme.uiFont
        font.pixelSize: Theme.captionFontSize
        // Eight is normally the widest proportional digit. The duration
        // metric below also covers longer-than-99-minute charts.
        text: "88:88.888"
    }

    TextMetrics {
        id: durationTimestampMetrics

        font.family: Theme.uiFont
        font.pixelSize: Theme.captionFontSize
        text: root.formatSecond(root.totalSeconds)
    }

    Text {
        id: timestamp

        objectName: root.objectNamePrefix + "Timestamp"
        width: root.timestampWidth
        horizontalAlignment: Text.AlignHCenter
        x: Math.max(0, Math.min(root.width - width,
                                lane.xForSecond(root.displaySecond) - width * 0.5))
        y: -height
        visible: mouseArea.containsMouse || root.draggingTarget.length > 0
        text: root.formatSecond(root.displaySecond)
        color: Theme.colors.text.primary
        font.family: Theme.uiFont
        font.pixelSize: Theme.captionFontSize
        z: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: 3
            color: Theme.overlayColor(Theme.colors.background.elevated, Theme.popupOpacity)
            z: -1
        }
    }

    Item {
        id: lane

        objectName: root.objectNamePrefix + "Lane"
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.timestampBandHeight
        height: 16

        readonly property real sideInset: 10
        readonly property real trackY: 5
        readonly property real trackHeight: 6
        readonly property real handleWidth: 14
        readonly property real handleHeight: 14
        readonly property real handleHitRadius: handleWidth * 0.5 + 3
        readonly property real minimumVisualSelectionWidth: handleWidth * 3
        readonly property int playheadWidthDevicePixels: 2
        // Use the lane's scene origin when snapping. mapToItem() is an
        // invokable rather than a property, so read it at each geometry
        // evaluation; caching it in a binding can retain the pre-layout origin.
        function sceneOriginX() {
            const mapped = lane.mapToItem(null, 0, 0)
            return mapped && isFinite(mapped.x) ? mapped.x : 0
        }
        // A fixed window carries the playhead through its drawn width, so it
        // stays inside the window even when the window is drawn wider than its
        // real span.
        readonly property bool playheadShown: !root.fixedLength
            || root.windowPlayheadSeconds >= 0
            || (Number(root.previewSession.positionSeconds) || 0) >= 0
        function playheadX() {
            if (root.fixedLength && root.windowPlayheadSeconds >= 0)
                return visualStartX + root.windowPlayheadSeconds / root.windowLengthSeconds()
                    * (visualEndX - visualStartX)
            return xForSecond(root.playheadSeconds)
        }
        readonly property int playheadCenterDeviceX: {
            const originX = sceneOriginX()
            return Math.round((originX + playheadX()) * root.renderDpr)
        }
        readonly property int playheadLeftDeviceX:
            playheadCenterDeviceX - Math.floor(playheadWidthDevicePixels * 0.5)
        readonly property int playheadRightDeviceX:
            playheadCenterDeviceX + Math.ceil(playheadWidthDevicePixels * 0.5)
        readonly property real playheadLeftX: {
            const originX = sceneOriginX()
            return playheadLeftDeviceX / root.renderDpr - originX
        }
        readonly property real playheadRightX: {
            return playheadLeftX + playheadWidth
        }
        readonly property real playheadWidth:
            playheadWidthDevicePixels / root.renderDpr
        readonly property real actualStartX: xForSecond(root.startSeconds)
        readonly property real actualEndX: xForSecond(root.endSeconds)
        readonly property real availableTrackWidth: Math.max(1, width - sideInset * 2)
        readonly property real visualSelectionWidth: Math.min(availableTrackWidth,
                                                               Math.max(minimumVisualSelectionWidth,
                                                                        actualEndX - actualStartX))
        readonly property real visualStartX: Math.max(sideInset,
                                                       Math.min(width - sideInset - visualSelectionWidth,
                                                                (actualStartX + actualEndX - visualSelectionWidth) * 0.5))
        readonly property real visualEndX: visualStartX + visualSelectionWidth
        readonly property bool rangeCanShift: root.totalSeconds > root.endSeconds - root.startSeconds

        function xForSecond(second) {
            const availableWidth = Math.max(1, width - sideInset * 2)
            if (root.totalSeconds <= 0)
                return sideInset
            return sideInset + Math.max(0, Math.min(1, second / root.totalSeconds)) * availableWidth
        }

        function unclampedSecondForX(x) {
            const availableWidth = Math.max(1, width - sideInset * 2)
            return (x - sideInset) / availableWidth * root.totalSeconds
        }

        function secondForX(x) {
            return Math.max(0, Math.min(root.totalSeconds, unclampedSecondForX(x)))
        }

        function targetAt(x, y) {
            if (root.fixedLength) {
                if (playheadShown && root.windowPlayheadSeconds >= 0 && Math.abs(x - playheadX()) <= 4)
                    return "playhead"
                if (x >= visualStartX - handleHitRadius && x <= visualEndX + handleHitRadius)
                    return "range"
                return rangeCanShift ? "jump" : ""
            }
            if (Math.abs(x - visualStartX) <= handleHitRadius)
                return "start"
            if (Math.abs(x - visualEndX) <= handleHitRadius)
                return "end"
            const inSelectedBody = x > visualStartX + handleHitRadius
                                && x < visualEndX - handleHitRadius
                                && y >= trackY && y <= trackY + trackHeight
            return inSelectedBody && rangeCanShift ? "range" : ""
        }

        Rectangle {
            x: lane.sideInset
            y: lane.trackY
            width: lane.availableTrackWidth
            height: lane.trackHeight
            radius: height * 0.5
            color: Theme.colors.border.control
        }

        Rectangle {
            id: rangeBody

            objectName: root.objectNamePrefix + "SelectedBody"
            x: lane.visualStartX
            y: lane.trackY
            width: Math.max(1, lane.visualEndX - x)
            height: lane.trackHeight
            radius: height * 0.5
            color: Theme.colors.accent.focus
        }

        Rectangle {
            id: playhead

            objectName: root.objectNamePrefix + "Playhead"
            visible: lane.playheadShown
            x: lane.playheadLeftX
            y: 1
            width: lane.playheadWidth
            height: lane.height - 2
            antialiasing: false
            color: Theme.colors.syntax.warning

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: -1
                width: 8
                height: 6
                rotation: 45
                antialiasing: false
                color: Theme.colors.syntax.warning
            }
        }

        Item {
            id: startHandle

            objectName: root.objectNamePrefix + "StartHandle"
            x: lane.visualStartX - width * 0.5
            y: (lane.height - height) * 0.5
            width: lane.handleWidth
            height: lane.handleHeight

            Rectangle {
                anchors.fill: parent
                radius: 3
                color: Theme.colors.accent.primary
                border.width: root.draggingTarget === "start" ? 2 : 0
                border.color: Theme.colors.accent.soft
            }

            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: parent.height - 8
                radius: 1
                color: Theme.colors.accent.soft
            }
        }

        Item {
            id: endHandle

            objectName: root.objectNamePrefix + "EndHandle"
            x: lane.visualEndX - width * 0.5
            y: (lane.height - height) * 0.5
            width: lane.handleWidth
            height: lane.handleHeight

            Rectangle {
                anchors.fill: parent
                radius: 3
                color: Theme.colors.accent.primary
                border.width: root.draggingTarget === "end" ? 2 : 0
                border.color: Theme.colors.accent.soft
            }

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: parent.height - 8
                radius: 1
                color: Theme.colors.accent.soft
            }
        }

        MouseArea {
            id: mouseArea

            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            cursorShape: root.draggingTarget === "range" ? Qt.ClosedHandCursor
                        : root.draggingTarget.length > 0 ? Qt.SizeHorCursor
                        : lane.targetAt(mouseX, mouseY) === "range" ? Qt.OpenHandCursor
                        : lane.targetAt(mouseX, mouseY) === "jump" ? Qt.PointingHandCursor
                        : lane.targetAt(mouseX, mouseY).length > 0 ? Qt.SizeHorCursor
                        : Qt.ArrowCursor

            onPressed: function(mouse) {
                root.hoverSecond = lane.secondForX(mouse.x)
                const target = lane.targetAt(mouse.x, mouse.y)
                if (target.length === 0)
                    return
                if (root.fixedLength) {
                    root.forceActiveFocus(Qt.MouseFocusReason)
                    root.beginWindowGesture(target, mouse.x)
                    return
                }
                root.beginDrag(target, lane.unclampedSecondForX(mouse.x))
            }
            onPositionChanged: function(mouse) {
                root.hoverSecond = lane.secondForX(mouse.x)
                if (!pressed || root.draggingTarget.length === 0)
                    return
                if (root.fixedLength)
                    root.updateWindowGesture(mouse.x)
                else
                    root.updateDrag(lane.unclampedSecondForX(mouse.x))
            }
            onReleased: root.fixedLength ? root.endWindowGesture() : root.endDrag()
            onCanceled: root.fixedLength ? root.endWindowGesture() : root.endDrag()
        }
    }

    Text {
        anchors.left: lane.left
        anchors.top: lane.bottom
        anchors.topMargin: 3
        text: root.formatSecond(0)
        color: Theme.colors.text.disabled
        font.family: Theme.uiFont
        font.pixelSize: Theme.captionFontSize
    }

    Text {
        anchors.right: lane.right
        anchors.top: lane.bottom
        anchors.topMargin: 3
        text: root.formatSecond(root.totalSeconds)
        color: Theme.colors.text.disabled
        font.family: Theme.uiFont
        font.pixelSize: Theme.captionFontSize
    }
}
