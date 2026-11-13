import QtQuick
import QtQuick.Layouts
import MiaCode.UI

// Label + slider + always-visible number field + unit, for a wide range whose
// common values sit in a narrower focus range (20–200 of 5–1000 MB). The focus
// range is linear and takes most of the track; the ends share the rest. The
// field takes any exact value.
RowLayout {
    id: root

    required property string label
    property int labelWidth: 120
    property real from: 1
    property real to: 100
    property real focusFrom: from
    property real focusTo: to
    // Track share of the focus range; each end gets half of the remainder.
    property real focusShare: 0.6
    property real value: from
    property string unit: ""
    // Fixed decimals, or -1 for "2 below 1, 1 below 10, else 0".
    property int decimals: 0
    // Fires on drags, keys and a committed field edit, never on value echoes.
    signal moved(real value)

    readonly property real edgeShare: (1 - root.focusShare) / 2

    function decimalsFor(v) {
        if (root.decimals >= 0)
            return root.decimals
        return v < 1 ? 2 : (v < 10 ? 1 : 0)
    }
    function rounded(v) {
        const clamped = Math.min(root.to, Math.max(root.from, v))
        const scale = Math.pow(10, root.decimalsFor(clamped))
        return Math.round(clamped * scale) / scale
    }
    // Track breakpoints: [position, value] pairs, linear between neighbours.
    function stops() {
        return [[0, root.from],
                [root.edgeShare, root.focusFrom],
                [1 - root.edgeShare, root.focusTo],
                [1, root.to]]
    }
    function positionOf(v) {
        const clamped = Math.min(root.to, Math.max(root.from, v))
        const s = root.stops()
        for (let i = 1; i < s.length; ++i) {
            if (clamped <= s[i][1] || i === s.length - 1) {
                const span = s[i][1] - s[i - 1][1]
                const t = span > 0 ? (clamped - s[i - 1][1]) / span : 0
                return s[i - 1][0] + t * (s[i][0] - s[i - 1][0])
            }
        }
        return 0
    }
    function valueAt(position) {
        const s = root.stops()
        for (let i = 1; i < s.length; ++i) {
            if (position <= s[i][0] || i === s.length - 1) {
                const span = s[i][0] - s[i - 1][0]
                const t = span > 0 ? (position - s[i - 1][0]) / span : 0
                return s[i - 1][1] + t * (s[i][1] - s[i - 1][1])
            }
        }
        return root.from
    }

    Layout.fillWidth: true
    spacing: 8

    Text {
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        Layout.preferredWidth: root.labelWidth
        text: root.label
        color: root.enabled ? Theme.colors.text.secondary : Theme.colors.text.disabled
        wrapMode: Text.WordWrap
    }
    AppSlider {
        id: slider
        // Auto-repeated arrow presses since the key went down.
        property int heldSteps: 0

        Layout.fillWidth: true
        from: 0
        to: 1
        Accessible.name: root.label
        onMoved: root.moved(root.rounded(root.valueAt(value)))

        // Arrows step by the displayed precision; holding the key speeds up
        // gradually (about 10x after one second) instead of jumping a tenth
        // of the track per press.
        Keys.onPressed: function(event) {
            const direction = (event.key === Qt.Key_Right || event.key === Qt.Key_Up) ? 1
                : ((event.key === Qt.Key_Left || event.key === Qt.Key_Down) ? -1 : 0)
            if (direction === 0 || event.modifiers !== Qt.NoModifier)
                return
            event.accepted = true
            slider.heldSteps = event.isAutoRepeat ? slider.heldSteps + 1 : 0
            const unitStep = Math.pow(10, -root.decimalsFor(root.value))
            const speed = Math.min(50, Math.pow(1.08, slider.heldSteps))
            const next = root.rounded(root.value + direction * unitStep * Math.round(speed))
            if (next !== root.value)
                root.moved(next)
        }

        // A drag writes `value` imperatively; re-apply the external value only
        // while the handle is released so the binding cannot fight the drag.
        Binding {
            target: slider
            property: "value"
            value: root.positionOf(root.value)
            when: !slider.pressed
            restoreMode: Binding.RestoreNone
        }
    }
    AppTextField {
        id: field
        Layout.preferredWidth: 76
        horizontalAlignment: Text.AlignRight
        validator: DoubleValidator {
            bottom: 0
            top: root.to
            decimals: 2
            notation: DoubleValidator.StandardNotation
        }
        onEditingFinished: {
            const typed = parseFloat(text)
            if (!isNaN(typed))
                root.moved(root.rounded(typed))
            text = root.value.toFixed(root.decimalsFor(root.value))
        }

        Binding {
            target: field
            property: "text"
            value: root.value.toFixed(root.decimalsFor(root.value))
            when: !field.activeFocus
            restoreMode: Binding.RestoreNone
        }
    }
    Text {
        Layout.preferredWidth: 36
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        text: root.unit
        color: root.enabled ? Theme.colors.text.secondary : Theme.colors.text.disabled
    }
}
