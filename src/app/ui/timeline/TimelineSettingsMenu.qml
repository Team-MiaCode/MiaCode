import QtQuick
import QtQuick.Controls
import MiaCode.UI

AppStickyPopup {
    id: root

    required property var stateBridge
    required property var timelineSession
    openAbove: true

    readonly property int brightnessPercentMin: 20
    readonly property int brightnessPercentMax: 200
    readonly property int brightnessPercentStep: 5
    readonly property bool spectrogramBusy: stateBridge ? stateBridge.spectrogramUpdating : false
    property bool spectrogramSettled: false
    property bool spectrogramBusySeen: false

    onSpectrogramBusyChanged: {
        if (spectrogramBusy) {
            spectrogramBusySeen = true
            spectrogramSettled = false
            return
        }
        if (!spectrogramBusySeen)
            return
        spectrogramBusySeen = false
        spectrogramSettled = true
        spectrogramSettledTimer.restart()
    }

    Timer {
        id: spectrogramSettledTimer
        interval: 1600
        onTriggered: root.spectrogramSettled = false
    }

    function percentFromBrightness(value) {
        const percent = Math.round(value * 100 / brightnessPercentStep) * brightnessPercentStep
        return Math.max(brightnessPercentMin, Math.min(brightnessPercentMax, percent))
    }

    component ParameterItem: Item {
        id: row

        required property string text
        required property string valueText
        required property real from
        required property real to
        required property real stepSize
        required property real value
        property bool commitOnRelease: false
        property string dragSuffix: ""
        property real dragValue: value
        signal edited(real value)
        signal committed(real value)

        implicitWidth: Math.max(188, 12 + titleMetrics.advanceWidth
                                + 8 + valueMetrics.advanceWidth + 16)
        implicitHeight: Theme.menuParameterRowHeight

        TextMetrics {
            id: titleMetrics
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            text: row.text
        }

        TextMetrics {
            id: valueMetrics
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            text: valueLabel.text
        }

        Column {
            id: body
            x: 12
            y: 4
            width: Math.max(0, row.width - 12 - 16)
            spacing: 0

            Item {
                width: parent.width
                implicitHeight: Theme.menuRowHeight

                Text {
                    id: titleLabel
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: valueLabel.left
                    anchors.rightMargin: 8
                    text: row.text
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    color: Theme.colors.text.primary
                    elide: Text.ElideRight
                }

                Text {
                    id: valueLabel
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.commitOnRelease && slider.pressed
                          ? Math.round(row.dragValue) + row.dragSuffix
                          : row.valueText
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    color: Theme.colors.text.secondary
                    horizontalAlignment: Text.AlignRight
                }
            }

            AppSlider {
                id: slider
                implicitHeight: Theme.menuRowHeight
                width: parent.width
                from: row.from
                to: row.to
                stepSize: row.stepSize
                snapMode: Slider.SnapAlways
                Accessible.name: row.text
                onMoved: {
                    row.dragValue = value
                    if (!row.commitOnRelease)
                        row.edited(value)
                }
                onPressedChanged: {
                    if (pressed) {
                        row.dragValue = value
                        return
                    }
                    if (row.commitOnRelease)
                        row.committed(row.dragValue)
                }

                // A plain `value:` binding is removed by the drag, and while it
                // is still live it writes the old value back over the handle.
                Binding {
                    target: slider
                    property: "value"
                    value: row.value
                    when: !slider.pressed
                    restoreMode: Binding.RestoreNone
                }
            }
        }
    }

    component ModeButton: Item {
        id: modeButton
        property string label
        property bool selected: false
        signal activated

        implicitWidth: modeLabel.implicitWidth + 20
        implicitHeight: Theme.menuRowHeight

        HoverChrome {
            anchors.fill: parent
            highlightOutset: 0
            stateColors: Theme.colors.popupState
            contentHeight: Theme.controlHighlightHeight - 2 * Theme.chromePadding
            selected: modeButton.selected
            hovered: modeMouse.containsMouse
            pressed: modeMouse.pressed
        }

        Text {
            id: modeLabel
            anchors.centerIn: parent
            text: modeButton.label
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            color: modeButton.selected ? Theme.colors.text.active : Theme.colors.text.secondary
        }

        MouseArea {
            id: modeMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: modeButton.activated()
        }
    }

    contentItem: Column {
        spacing: 0

        Item {
            id: visualizationRow
            readonly property int mode: root.stateBridge ? root.stateBridge.audioVisualizationMode : 0
            implicitWidth: Math.max(240, 12 + titleMetrics.advanceWidth
                                    + 16 + waveformMetrics.advanceWidth + 20
                                    + spectrogramMetrics.advanceWidth + 20 + 16
                                    + (mode === 1 ? 22 : 0))
            implicitHeight: Theme.menuRowHeight

            TextMetrics {
                id: titleMetrics
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                text: qsTrId("shell.timeline_audio_visualization")
            }
            TextMetrics {
                id: waveformMetrics
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                text: qsTrId("shell.timeline_audio_visualization_waveform")
            }
            TextMetrics {
                id: spectrogramMetrics
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                text: qsTrId("shell.timeline_audio_visualization_spectrogram")
            }

            Text {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, visualizationRow.width - 12 - 16
                                - waveformMetrics.advanceWidth - 20
                                - spectrogramMetrics.advanceWidth - 20 - 8
                                - (visualizationRow.mode === 1 ? 22 : 0))
                text: titleMetrics.text
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                color: Theme.colors.text.primary
                elide: Text.ElideRight
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Item {
                    id: spectrogramHelp
                    visible: visualizationRow.mode === 1
                    width: 16
                    implicitHeight: Theme.menuRowHeight

                    Rectangle {
                        id: spectrogramHelpMark
                        width: 16
                        height: 16
                        anchors.centerIn: parent
                        radius: width / 2
                        color: "transparent"
                        border.width: Theme.controlBorderWidth
                        border.color: spectrogramHelpMouse.containsMouse
                                      ? Theme.colors.text.active
                                      : Theme.colors.text.secondary

                        Text {
                            anchors.centerIn: parent
                            text: "?"
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.uiFontSize - 1
                            font.bold: true
                            color: parent.border.color
                        }

                        MouseArea {
                            id: spectrogramHelpMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTrId("shell.timeline_spectrogram_reading_help")
                        }

                        Tooltip {
                            visible: spectrogramHelpMouse.containsMouse
                            timeout: -1
                            text: qsTrId("shell.timeline_spectrogram_reading_help")
                        }
                    }
                }

                ModeButton {
                    label: waveformMetrics.text
                    selected: visualizationRow.mode === 0
                    Accessible.name: label
                    Accessible.role: Accessible.Button
                    onActivated: {
                        if (root.stateBridge)
                            root.stateBridge.audioVisualizationMode = 0
                    }
                }

                ModeButton {
                    label: spectrogramMetrics.text
                    selected: visualizationRow.mode === 1
                    Accessible.name: label
                    Accessible.role: Accessible.Button
                    onActivated: {
                        if (root.stateBridge)
                            root.stateBridge.audioVisualizationMode = 1
                    }
                }
            }
        }

        Item {
            width: parent.width
            visible: visualizationRow.mode === 1 && (root.spectrogramBusy || root.spectrogramSettled)
            implicitHeight: Theme.menuRowHeight

            BusyIndicator {
                x: 12
                width: 14
                height: 14
                anchors.verticalCenter: parent.verticalCenter
                running: root.spectrogramBusy
                visible: running
            }

            Text {
                x: root.spectrogramBusy ? 32 : 12
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - x - 16)
                text: root.spectrogramBusy
                      ? qsTrId("shell.timeline_spectrogram_updating")
                      : qsTrId("shell.timeline_spectrogram_updated")
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                color: Theme.colors.text.secondary
                elide: Text.ElideRight
            }
        }

        Item {
            id: frequencyRange
            width: parent.width
            visible: visualizationRow.mode === 1
            readonly property real minHz: root.stateBridge ? root.stateBridge.spectrogramMinHz : 20
            readonly property real maxHz: root.stateBridge ? root.stateBridge.spectrogramMaxHz : 1000
            readonly property real logFrom: Math.log(20)
            readonly property real logTo: Math.log(16000)
            readonly property var snapMarks: [20, 40, 500, 1000, 2000, 4000, 8000, 16000]
            property real dragMin: minHz
            property real dragMax: maxHz
            property bool adjusting: false

            function formatHz(hz) {
                const rounded = Math.round(hz)
                if (rounded >= 1000 && rounded % 1000 === 0)
                    return (rounded / 1000) + " kHz"
                if (rounded >= 1000)
                    return (rounded / 1000).toFixed(1) + " kHz"
                return rounded + " Hz"
            }

            function logOf(hz) {
                return Math.log(Math.max(20, Math.min(16000, hz)))
            }

            function hzOf(logValue) {
                return Math.exp(logValue)
            }

            function magnet(hz) {
                let nearest = hz
                let nearestDistance = Infinity
                for (let index = 0; index < snapMarks.length; ++index) {
                    const mark = snapMarks[index]
                    const distance = Math.abs(Math.log(hz) - Math.log(mark))
                    if (distance < nearestDistance) {
                        nearestDistance = distance
                        nearest = mark
                    }
                }
                return nearestDistance <= Math.log(1.15) ? nearest : Math.round(hz)
            }

            property bool dragging: false
            property bool snapActive: false

            function placeHandle(handle, hz) {
                const logValue = logOf(hz)
                if (Math.abs(handle.value - logValue) <= 0.0005)
                    return
                adjusting = true
                handle.value = logValue
                adjusting = false
            }

            function resolvedHz(logValue, snap) {
                const hz = Math.max(20, Math.min(16000, hzOf(logValue)))
                return snap ? magnet(hz) : Math.round(hz)
            }

            function commitWindow() {
                if (!root.stateBridge)
                    return
                let low = resolvedHz(frequencySlider.first.value, snapActive)
                let high = resolvedHz(frequencySlider.second.value, snapActive)
                if (high < low * 2)
                    high = Math.min(16000, Math.round(low * 2))
                root.stateBridge.setSpectrogramFrequencyWindow(low, high)
                snapActive = false
                dragging = false
            }

            implicitWidth: Math.max(220, 12 + frequencyTitle.advanceWidth + 8 + frequencyValue.advanceWidth + 16)
            implicitHeight: Theme.menuParameterRowHeight

            TextMetrics {
                id: frequencyTitle
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                text: qsTrId("shell.timeline_spectrogram_frequency_range")
            }
            TextMetrics {
                id: frequencyValue
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                text: frequencyReadout.text
            }

            Column {
                x: 12
                y: 4
                width: Math.max(0, frequencyRange.width - 12 - 16)
                spacing: 0

                Item {
                    width: parent.width
                    implicitHeight: Theme.menuRowHeight

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: frequencyReadout.left
                        anchors.rightMargin: 8
                        text: frequencyTitle.text
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        color: Theme.colors.text.primary
                        elide: Text.ElideRight
                    }

                    Text {
                        id: frequencyReadout
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: frequencyRange.formatHz(frequencyRange.dragging
                                                       ? frequencyRange.dragMin : frequencyRange.minHz)
                              + " – "
                              + frequencyRange.formatHz(frequencyRange.dragging
                                                        ? frequencyRange.dragMax : frequencyRange.maxHz)
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        color: Theme.colors.text.secondary
                        horizontalAlignment: Text.AlignRight
                    }
                }

                RangeSlider {
                    id: frequencySlider
                    implicitHeight: Theme.menuRowHeight
                    width: parent.width
                    padding: 0
                    readonly property real knob: 12
                    // The colored span and the tick marks use this same center,
                    // so a handle sits on the value instead of beside the track.
                    function knobCenter(visualPosition) {
                        return leftPadding + visualPosition * Math.max(0, availableWidth - knob) + knob / 2
                    }
                    from: frequencyRange.logFrom
                    to: frequencyRange.logTo
                    Accessible.name: frequencyTitle.text

                    Binding {
                        target: frequencySlider.first
                        property: "value"
                        value: frequencyRange.logOf(frequencyRange.minHz)
                        when: !frequencyRange.dragging
                        restoreMode: Binding.RestoreNone
                    }
                    Binding {
                        target: frequencySlider.second
                        property: "value"
                        value: frequencyRange.logOf(frequencyRange.maxHz)
                        when: !frequencyRange.dragging
                        restoreMode: Binding.RestoreNone
                    }

                    background: Item {
                        Rectangle {
                            id: frequencyTrack
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 4
                            radius: 2
                            color: Theme.overlayColor(Theme.colors.slider.rail)

                            Rectangle {
                                x: frequencySlider.knobCenter(frequencySlider.first.visualPosition) - frequencyTrack.x
                                width: Math.max(0, frequencySlider.knobCenter(frequencySlider.second.visualPosition)
                                                   - frequencySlider.knobCenter(frequencySlider.first.visualPosition))
                                height: parent.height
                                radius: parent.radius
                                color: Theme.colors.slider.fill
                            }
                        }

                        Repeater {
                            model: frequencyRange.snapMarks
                            Rectangle {
                                readonly property real fraction: (Math.log(modelData) - frequencyRange.logFrom)
                                                                  / (frequencyRange.logTo - frequencyRange.logFrom)
                                x: frequencySlider.knobCenter(fraction) - width / 2
                                y: frequencyTrack.y - height
                                width: 2
                                height: 6
                                radius: 1
                                color: Theme.colors.accent.primary
                            }
                        }
                    }

                    first.handle: Rectangle {
                        x: frequencySlider.knobCenter(frequencySlider.first.visualPosition) - width / 2
                        y: frequencySlider.topPadding + frequencySlider.availableHeight / 2 - height / 2
                        implicitWidth: frequencySlider.knob
                        implicitHeight: frequencySlider.knob
                        width: frequencySlider.knob
                        height: frequencySlider.knob
                        radius: frequencySlider.knob / 2
                        color: Theme.colors.slider.handle
                        border.width: Theme.controlBorderWidth
                        border.color: Theme.colors.toggle.knobBorder
                    }
                    second.handle: Rectangle {
                        x: frequencySlider.knobCenter(frequencySlider.second.visualPosition) - width / 2
                        y: frequencySlider.topPadding + frequencySlider.availableHeight / 2 - height / 2
                        implicitWidth: frequencySlider.knob
                        implicitHeight: frequencySlider.knob
                        width: frequencySlider.knob
                        height: frequencySlider.knob
                        radius: frequencySlider.knob / 2
                        color: Theme.colors.slider.handle
                        border.width: Theme.controlBorderWidth
                        border.color: Theme.colors.toggle.knobBorder
                    }

                    // Owns the drag so the pointer can leave the track. Snapping
                    // happens only while that pointer is above the track, where
                    // the tick marks sit. On the track the value follows the pointer.
                    MouseArea {
                        id: frequencyDrag
                        anchors.fill: parent
                        z: 2
                        preventStealing: true
                        property bool dragSecond: false

                        function aboveTrack(y) {
                            return y < frequencyTrack.mapToItem(frequencySlider, 0, 0).y
                        }

                        function logAt(x) {
                            const travel = Math.max(1, frequencySlider.availableWidth - frequencySlider.knob)
                            const fraction = Math.max(0, Math.min(1,
                                (x - frequencySlider.leftPadding - frequencySlider.knob / 2) / travel))
                            return frequencySlider.from + fraction * (frequencySlider.to - frequencySlider.from)
                        }

                        function moveActiveHandle(x, y) {
                            frequencyRange.dragging = true
                            frequencyRange.snapActive = aboveTrack(y)
                            let hz = frequencyRange.resolvedHz(logAt(x), frequencyRange.snapActive)
                            if (dragSecond) {
                                const floorHz = frequencyRange.hzOf(frequencySlider.first.value)
                                if (hz < floorHz)
                                    hz = floorHz
                                frequencyRange.placeHandle(frequencySlider.second, hz)
                            } else {
                                const capHz = frequencyRange.hzOf(frequencySlider.second.value)
                                if (hz > capHz)
                                    hz = capHz
                                frequencyRange.placeHandle(frequencySlider.first, hz)
                            }
                            frequencyRange.dragMin = frequencyRange.hzOf(frequencySlider.first.value)
                            frequencyRange.dragMax = frequencyRange.hzOf(frequencySlider.second.value)
                        }

                        onPressed: function(mouse) {
                            const firstX = frequencySlider.knobCenter(frequencySlider.first.visualPosition)
                            const secondX = frequencySlider.knobCenter(frequencySlider.second.visualPosition)
                            dragSecond = Math.abs(mouse.x - secondX) <= Math.abs(mouse.x - firstX)
                            moveActiveHandle(mouse.x, mouse.y)
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed)
                                moveActiveHandle(mouse.x, mouse.y)
                        }
                        onReleased: frequencyRange.commitWindow()
                    }
                }
            }
        }

        Item {
            id: loudnessRange
            width: parent.width
            visible: visualizationRow.mode === 1
            readonly property real floorDb: root.stateBridge ? root.stateBridge.spectrogramFloorDb : -30
            readonly property real ceilingDb: root.stateBridge ? root.stateBridge.spectrogramCeilingDb : 0
            readonly property string restingText: Math.round(floorDb) + " – " + Math.round(ceilingDb) + " dB"
            property real dragFloor: floorDb
            property real dragCeiling: ceilingDb

            function commitWindow() {
                if (!root.stateBridge)
                    return
                root.stateBridge.setSpectrogramLoudnessWindow(dragFloor, dragCeiling)
            }

            implicitWidth: Math.max(188, 12 + loudnessTitle.advanceWidth + 8 + loudnessValue.advanceWidth + 16)
            implicitHeight: Theme.menuParameterRowHeight

            TextMetrics {
                id: loudnessTitle
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                text: qsTrId("shell.timeline_spectrogram_loudness_range")
            }
            TextMetrics {
                id: loudnessValue
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                text: loudnessReadout.text
            }

            Column {
                x: 12
                y: 4
                width: Math.max(0, loudnessRange.width - 12 - 16)
                spacing: 0

                Item {
                    width: parent.width
                    implicitHeight: Theme.menuRowHeight

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: loudnessReadout.left
                        anchors.rightMargin: 8
                        text: loudnessTitle.text
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        color: Theme.colors.text.primary
                        elide: Text.ElideRight
                    }

                    Text {
                        id: loudnessReadout
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: loudnessSlider.first.pressed || loudnessSlider.second.pressed
                              ? Math.round(loudnessRange.dragFloor) + " – " + Math.round(loudnessRange.dragCeiling) + " dB"
                              : loudnessRange.restingText
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        color: Theme.colors.text.secondary
                        horizontalAlignment: Text.AlignRight
                    }
                }

                RangeSlider {
                    id: loudnessSlider
                    implicitHeight: Theme.menuRowHeight
                    width: parent.width
                    padding: 0
                    readonly property real knob: 12
                    function knobCenter(visualPosition) {
                        return leftPadding + visualPosition * Math.max(0, availableWidth - knob) + knob / 2
                    }
                    from: -80
                    to: 12
                    stepSize: 1
                    snapMode: RangeSlider.SnapAlways
                    Accessible.name: loudnessTitle.text

                    function noteDrag() {
                        loudnessRange.dragFloor = first.value
                        loudnessRange.dragCeiling = second.value
                    }
                    first.onMoved: loudnessSlider.noteDrag()
                    second.onMoved: loudnessSlider.noteDrag()
                    first.onPressedChanged: {
                        if (first.pressed) {
                            loudnessSlider.noteDrag()
                            return
                        }
                        if (!second.pressed)
                            loudnessRange.commitWindow()
                    }
                    second.onPressedChanged: {
                        if (second.pressed) {
                            loudnessSlider.noteDrag()
                            return
                        }
                        if (!first.pressed)
                            loudnessRange.commitWindow()
                    }

                    Binding {
                        target: loudnessSlider.first
                        property: "value"
                        value: loudnessRange.floorDb
                        when: !loudnessSlider.first.pressed && !loudnessSlider.second.pressed
                        restoreMode: Binding.RestoreNone
                    }
                    Binding {
                        target: loudnessSlider.second
                        property: "value"
                        value: loudnessRange.ceilingDb
                        when: !loudnessSlider.first.pressed && !loudnessSlider.second.pressed
                        restoreMode: Binding.RestoreNone
                    }

                    background: Item {
                        Rectangle {
                            id: loudnessTrack
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 4
                            radius: 2
                            color: Theme.overlayColor(Theme.colors.slider.rail)

                            Rectangle {
                                x: loudnessSlider.knobCenter(loudnessSlider.first.visualPosition) - loudnessTrack.x
                                width: Math.max(0, loudnessSlider.knobCenter(loudnessSlider.second.visualPosition)
                                                   - loudnessSlider.knobCenter(loudnessSlider.first.visualPosition))
                                height: parent.height
                                radius: parent.radius
                                color: Theme.colors.slider.fill
                            }
                        }
                    }

                    first.handle: Rectangle {
                        x: loudnessSlider.knobCenter(loudnessSlider.first.visualPosition) - width / 2
                        y: loudnessSlider.topPadding + loudnessSlider.availableHeight / 2 - height / 2
                        implicitWidth: loudnessSlider.knob
                        implicitHeight: loudnessSlider.knob
                        width: loudnessSlider.knob
                        height: loudnessSlider.knob
                        radius: loudnessSlider.knob / 2
                        color: Theme.colors.slider.handle
                        border.width: Theme.controlBorderWidth
                        border.color: Theme.colors.toggle.knobBorder
                    }
                    second.handle: Rectangle {
                        x: loudnessSlider.knobCenter(loudnessSlider.second.visualPosition) - width / 2
                        y: loudnessSlider.topPadding + loudnessSlider.availableHeight / 2 - height / 2
                        implicitWidth: loudnessSlider.knob
                        implicitHeight: loudnessSlider.knob
                        width: loudnessSlider.knob
                        height: loudnessSlider.knob
                        radius: loudnessSlider.knob / 2
                        color: Theme.colors.slider.handle
                        border.width: Theme.controlBorderWidth
                        border.color: Theme.colors.toggle.knobBorder
                    }
                }
            }
        }

        ParameterItem {
            width: parent.width
            visible: visualizationRow.mode === 1
            text: qsTrId("shell.timeline_spectrogram_column_gate_db")
            readonly property real db: root.stateBridge ? root.stateBridge.spectrogramColumnGateDb : -30
            valueText: Math.round(db) + " dB"
            dragSuffix: " dB"
            from: -80
            to: -12
            stepSize: 1
            value: db
            commitOnRelease: true
            onCommitted: value => root.stateBridge.spectrogramColumnGateDb = value
        }

        ParameterItem {
            width: parent.width
            visible: visualizationRow.mode === 1
            text: qsTrId("shell.timeline_spectrogram_prominence_floor_db")
            readonly property real db: root.stateBridge ? root.stateBridge.spectrogramProminenceFloorDb : -32
            valueText: Math.round(db) + " dB"
            dragSuffix: " dB"
            from: -80
            to: -12
            stepSize: 1
            value: db
            commitOnRelease: true
            onCommitted: value => root.stateBridge.spectrogramProminenceFloorDb = value
        }

        ParameterItem {
            width: parent.width
            visible: visualizationRow.mode === 1
            text: qsTrId("shell.timeline_spectrogram_opacity")
            readonly property real amount: root.stateBridge ? root.stateBridge.spectrogramOpacity : 0.7
            valueText: qsTrId("qml.1").arg(Math.round(amount * 100))
            from: 0
            to: 100
            stepSize: 5
            value: Math.round(amount * 100)
            onEdited: value => root.stateBridge.spectrogramOpacity = value / 100
        }

        ParameterItem {
            width: parent.width
            visible: visualizationRow.mode !== 1
            text: qsTrId("shell.timeline_waveform_brightness")
            readonly property real brightness: root.stateBridge ? root.stateBridge.waveformBrightness : 0.5
            valueText: qsTrId("qml.1").arg(Math.round(brightness * 100))
            from: root.brightnessPercentMin
            to: root.brightnessPercentMax
            stepSize: root.brightnessPercentStep
            value: root.percentFromBrightness(brightness)
            onEdited: value => root.stateBridge.waveformBrightness = value / 100
        }

        ParameterItem {
            width: parent.width
            visible: visualizationRow.mode !== 1
            text: qsTrId("shell.timeline_measure_line_brightness")
            readonly property real brightness: root.stateBridge ? root.stateBridge.measureLineBrightness : 1.0
            valueText: qsTrId("qml.1").arg(Math.round(brightness * 100))
            from: root.brightnessPercentMin
            to: root.brightnessPercentMax
            stepSize: root.brightnessPercentStep
            value: root.percentFromBrightness(brightness)
            onEdited: value => root.stateBridge.measureLineBrightness = value / 100
        }

        AppMenuSeparator { width: parent.width }

        AppSwitch {
            id: followSwitch
            width: parent.width
            implicitHeight: Theme.menuRowHeight
            leftPadding: 12
            rightPadding: 16
            text: root.timelineSession.followCodeLabel
            checked: root.stateBridge && root.stateBridge.followPreviewEnabled
            Accessible.description: qsTrId("qml.follow_current_chart_source_position")
            onToggled: {
                if (root.stateBridge && checked !== root.stateBridge.followPreviewEnabled)
                    root.timelineSession.followPreviewToggled(checked)
            }
            Connections {
                target: root.stateBridge
                function onFollowPreviewEnabledChanged() {
                    followSwitch.checked = root.stateBridge.followPreviewEnabled
                }
            }
        }
    }
}
