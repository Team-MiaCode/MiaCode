import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// 谱面整理选项；调用方执行整理事务。
AppDialog {
    id: root

    // Human-readable description of what will be normalized (whole chart, or a
    // line/column range), supplied by the caller.
    property string selectionDescription: ""

    // 选项由文档模型提供，显示名称使用“保留分段”和“紧凑格式”。
    required property var documentSession

    // Seed values; the caller reads the same-named properties back on accept.
    property bool reduceTo384Grid: true
    property int sectionMeasureCount: 4
    property string syntax: "segment_preserving"

    readonly property var gridOptions: documentSession.normalizationGridOptions
    readonly property var sectionOptions: documentSession.normalizationSectionOptions
    readonly property var syntaxOptions: documentSession.normalizationSyntaxOptions

    function indexOfValue(options, value) {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === value)
                return i
        }
        return 0
    }

    title: qsTrId("qml.normalize_whole_chart")
    preferredWidth: 420
    preferredHeight: implicitHeight
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

    body: ColumnLayout {
        spacing: 12

        Text {
            objectName: "normalizeSelectionDescription"
            Layout.fillWidth: true
            text: root.selectionDescription
            color: Theme.colors.text.secondary
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTrId("qml.align_to_the_1_384_grid")
                color: Theme.colors.text.secondary
            }
            AppComboBox {
                id: reduceCombo
                objectName: "normalizeReduce384Combo"
                Layout.fillWidth: true
                textRole: "label"
                model: root.gridOptions
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTrId("document.chart_sectioning")
                color: Theme.colors.text.secondary
            }
            AppComboBox {
                id: sectionCombo
                objectName: "normalizeSectionCombo"
                Layout.fillWidth: true
                textRole: "label"
                model: root.sectionOptions
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTrId("qml.format_syntax")
                color: Theme.colors.text.secondary
            }
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
