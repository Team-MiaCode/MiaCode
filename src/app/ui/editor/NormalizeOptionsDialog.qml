import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// 谱面整理选项；调用方执行整理事务。
AppDialog {
    id: root

    property string selectionDescription: ""
    required property var documentSession
    property bool reduceTo384Grid: true
    property int sectionMeasureCount: 4
    property string syntax: "segment_preserving"

    readonly property var gridOptions: documentSession.normalizationGridOptions
    readonly property var sectionOptions: documentSession.normalizationSectionOptions
    readonly property var syntaxOptions: documentSession.normalizationSyntaxOptions

    function indexOfValue(options, value) {
        for (let i = 0; i < options.length; ++i)
            if (options[i].value === value)
                return i
        return 0
    }

    title: qsTrId("qml.normalize_whole_chart")
    preferredWidth: 440
    preferredHeight: Math.max(220, implicitHeight)
    footer: DialogFooter {
        acceptText: qsTrId("dialog.normalize.apply")
        cancelText: qsTrId("action.cancel")
        onAccepted: root.accept()
        onRejected: root.reject()
    }

    onAccepted: {
        root.reduceTo384Grid = root.gridOptions[reduceCombo.currentIndex].value
        root.sectionMeasureCount = root.sectionOptions[sectionCombo.currentIndex].value
        root.syntax = root.syntaxOptions[syntaxCombo.currentIndex].value
    }

    onAboutToShow: {
        reduceCombo.currentIndex = root.indexOfValue(root.gridOptions, root.reduceTo384Grid)
        sectionCombo.currentIndex = root.indexOfValue(root.sectionOptions, root.sectionMeasureCount)
        syntaxCombo.currentIndex = root.indexOfValue(root.syntaxOptions, root.syntax)
    }

    component FieldLabel: Text {
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
    }

    body: ColumnLayout {
        spacing: Theme.settingsRowSpacing

        Text {
            objectName: "normalizeSelectionDescription"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.selectionDescription
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.secondaryFontSize
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: Theme.settingsRowSpacing

            FieldLabel { text: qsTrId("qml.align_to_the_1_384_grid") }
            AppComboBox {
                id: reduceCombo
                objectName: "normalizeReduce384Combo"
                Layout.fillWidth: true
                textRole: "label"
                model: root.gridOptions
            }

            FieldLabel { text: qsTrId("document.chart_sectioning") }
            AppComboBox {
                id: sectionCombo
                objectName: "normalizeSectionCombo"
                Layout.fillWidth: true
                textRole: "label"
                model: root.sectionOptions
            }

            FieldLabel { text: qsTrId("qml.format_syntax") }
            AppComboBox {
                id: syntaxCombo
                objectName: "normalizeSyntaxCombo"
                Layout.fillWidth: true
                textRole: "label"
                model: root.syntaxOptions
            }
        }
    }
}
