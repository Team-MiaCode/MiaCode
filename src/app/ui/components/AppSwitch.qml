import QtQuick
import QtQuick.Controls
import MiaCode.UI

// Themed switch — geometry/colors aligned with v2 Theme (not stock Fusion).
// Off is neutral and on is a muted accent (Theme toggle colors), so a stack of
// options stays quieter than the page's primary action.
Switch {
    id: root

    // A switch that heads a group of settings (片头 → 添加片头) reads as that
    // group's section caption rather than as one row inside it.
    property bool sectionTitle: false

    font.family: Theme.uiFont
    font.pixelSize: root.sectionTitle ? Theme.sectionTitleFontSize : Theme.uiFontSize
    hoverEnabled: true
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    implicitHeight: Theme.controlMinHeight
    implicitWidth: Math.ceil(leftPadding + rightPadding
                             + (indicator ? indicator.implicitWidth : 0)
                             + (root.text.length > 0 ? spacing + labelMetrics.advanceWidth : 0))

    TextMetrics {
        id: labelMetrics
        font: root.font
        text: root.text
    }

    indicator: Rectangle {
        implicitWidth: 28
        implicitHeight: 16
        x: root.leftPadding
        y: parent.height / 2 - height / 2
        radius: height / 2
        color: Theme.overlayColor(root.checked ? Theme.colors.toggle.checkedTrack
                                               : Theme.colors.toggle.track)
        opacity: root.enabled ? 1 : 0.45

        Rectangle {
            x: root.checked ? parent.width - width - 2 : 2
            anchors.verticalCenter: parent.verticalCenter
            width: 12
            height: 12
            radius: 6
            color: root.checked ? Theme.colors.toggle.checkedKnob : Theme.colors.toggle.knob
            border.width: 1
            border.color: Theme.colors.toggle.knobBorder
            Behavior on x { NumberAnimation { duration: 100 } }
        }
    }

    contentItem: Text {
        text: root.text
        font: root.font
        // One label color for both states, matching the form's row labels, so
        // an option never reads like the section caption above it.
        color: !root.enabled ? Theme.colors.text.disabled
               : root.sectionTitle ? Theme.colors.text.section
               : Theme.colors.text.secondary
        verticalAlignment: Text.AlignVCenter
        leftPadding: root.indicator.width + root.spacing
    }
}
