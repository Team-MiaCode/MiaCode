import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

ApplicationWindow {
    id: window

    required property var controller

    readonly property string monoFamily: Qt.platform.os === "windows" ? "Consolas"
                                       : Qt.platform.os === "osx" ? "Menlo" : "monospace"
    readonly property real hairline: 1 / Screen.devicePixelRatio
    property string filterText: ""
    property bool showMessages: false
    property bool lastRunWrote: false

    // Theme colors are stored as strings; Qt.lighter(…, 1) yields a color value.
    function tint(color, alpha) {
        const value = Qt.lighter(color, 1.0)
        return Qt.rgba(value.r, value.g, value.b, alpha)
    }

    function statusColor(status) {
        switch (status) {
        case "changed": return Theme.colors.syntax.comment
        case "unchanged": return Theme.colors.text.disabled
        case "warning": return Theme.colors.syntax.warning
        case "failed": return Theme.colors.danger.primary
        case "written": return Theme.colors.accent.primary
        default: return Theme.colors.text.secondary
        }
    }

    function matchesFilter(title, path) {
        const needle = window.filterText.trim().toLowerCase()
        return needle.length === 0 || title.toLowerCase().indexOf(needle) >= 0
            || path.toLowerCase().indexOf(needle) >= 0
    }

    width: 1180
    height: 760
    minimumWidth: 920
    minimumHeight: 600
    visible: true
    title: "MiaCode 谱面批处理"
    color: Theme.colors.background.surface
    font.family: Theme.uiFont
    font.pixelSize: Theme.uiFontSize

    palette.window: Theme.colors.background.surface
    palette.windowText: Theme.colors.text.primary
    palette.base: Theme.colors.background.surface
    palette.alternateBase: Theme.colors.background.elevated
    palette.text: Theme.colors.text.primary
    palette.button: Theme.colors.background.panel
    palette.buttonText: Theme.colors.text.primary
    palette.light: Theme.colors.border.control
    palette.midlight: Theme.colors.border.normal
    palette.mid: Theme.colors.border.control
    palette.dark: Theme.colors.background.surface
    palette.shadow: "#000000"
    palette.highlight: Theme.colors.state.textSelection
    palette.highlightedText: Theme.colors.text.active
    palette.placeholderText: Theme.colors.text.secondary
    palette.toolTipBase: Theme.overlayColor(Theme.colors.background.elevated, Theme.popupOpacity)
    palette.toolTipText: Theme.colors.text.primary
    palette.disabled.text: Theme.colors.text.disabled
    palette.disabled.windowText: Theme.colors.text.disabled
    palette.disabled.buttonText: Theme.colors.text.disabled

    // What Theme reads from preferences: the application's light palette and
    // the process UI font; the tool has no appearance preferences of its own.
    QtObject {
        id: toolPreferences
        readonly property string activeThemeToken: "light"
        readonly property bool darkTheme: false
        readonly property bool blurMaterialsEnabled: true
        readonly property string uiFontFamily: Qt.application.font.family
        readonly property font codeFont: Qt.font({ family: window.monoFamily, pixelSize: 13 })
        readonly property int editorBlockSpacing: 0
        readonly property int fontSize: 13
        readonly property int uiScalePercent: 100
    }

    Binding {
        target: Theme
        property: "preferences"
        value: toolPreferences
    }

    Connections {
        target: window.controller
        function onBusyChanged() {
            if (window.controller.busy)
                window.lastRunWrote = window.controller.writing
        }
    }

    FolderDialog {
        id: inputFolderDialog
        title: "添加曲目文件夹"
        onAccepted: window.controller.addUrls([selectedFolder])
    }

    FileDialog {
        id: inputFileDialog
        title: "添加谱面文件"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["谱面文件 (*.txt *.simai)", "所有文件 (*)"]
        onAccepted: window.controller.addUrls(selectedFiles)
    }

    FolderDialog {
        id: outputDialog
        title: "选择输出目录"
        currentFolder: window.controller.urlFromPath(window.controller.outputRoot)
        onAccepted: window.controller.outputRoot = window.controller.pathFromUrl(selectedFolder)
    }

    Timer {
        id: designerCommit
        interval: 300
        onTriggered: window.controller.designer = designerField.text
    }

    component Hairline: Rectangle {
        color: Theme.separatorColor
        implicitWidth: window.hairline
        implicitHeight: window.hairline
    }


    component OptionCombo: RowLayout {
        id: optionCombo
        property alias label: comboLabel.text
        property alias model: combo.model
        property alias currentIndex: combo.currentIndex
        signal activated(int index)
        Layout.fillWidth: true
        spacing: Theme.settingsRowSpacing

        Text {
            id: comboLabel
            color: Theme.colors.text.primary
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
        }
        AppComboBox {
            id: combo
            Layout.fillWidth: true
            onActivated: (index) => optionCombo.activated(index)
        }
    }

    component DiffColumn: ColumnLayout {
        id: diffColumn
        property string caption
        property var rows: []
        property color markColor
        spacing: 4

        Text {
            text: diffColumn.caption
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.captionFontSize + 1
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.colors.background.control
            border.color: Theme.colors.border.normal
            border.width: 1
            radius: Theme.smallControlRadius
            clip: true

            ListView {
                id: diffList
                anchors.fill: parent
                anchors.margins: 6
                model: diffColumn.rows
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar {}
                delegate: Rectangle {
                    required property var modelData
                    width: diffList.width
                    implicitHeight: lineText.implicitHeight + 2
                    color: modelData.marked ? diffColumn.markColor : "transparent"

                    Text {
                        id: lineText
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.text.length > 0 ? modelData.text : " "
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        color: Theme.colors.text.editor
                        font.family: window.monoFamily
                        font.pixelSize: Theme.uiFontSize - 1
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ---- queue -------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "待处理"
                        color: Theme.colors.text.section
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.sectionTitleFontSize
                        font.weight: Theme.sectionTitleFontWeight
                    }
                    Rectangle {
                        visible: window.controller.jobCount > 0
                        implicitWidth: countText.implicitWidth + 12
                        implicitHeight: 18
                        radius: height / 2
                        color: Theme.colors.accent.badge
                        Text {
                            id: countText
                            anchors.centerIn: parent
                            text: window.controller.checkedCount + " / " + window.controller.jobCount
                            color: Theme.colors.text.onAccent
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.secondaryFontSize
                        }
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: "添加文件夹"
                        enabled: !window.controller.busy
                        onClicked: inputFolderDialog.open()
                    }
                    AppButton {
                        text: "添加文件"
                        enabled: !window.controller.busy
                        onClicked: inputFileDialog.open()
                    }
                    AppButton {
                        text: "移除勾选"
                        enabled: !window.controller.busy && window.controller.checkedCount > 0
                        onClicked: window.controller.removeChecked()
                    }
                    AppButton {
                        text: "清空"
                        enabled: !window.controller.busy && window.controller.jobCount > 0
                        onClicked: window.controller.clear()
                    }
                }

                AppTextField {
                    Layout.fillWidth: true
                    placeholderText: "筛选曲名或路径"
                    onTextChanged: window.filterText = text
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    spacing: 8

                    AppCheckBox {
                        checkable: false
                        enabled: !window.controller.busy && window.controller.jobCount > 0
                        checked: window.controller.jobCount > 0
                                 && window.controller.checkedCount === window.controller.jobCount
                        onClicked: window.controller.setAllChecked(!checked)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "曲目"
                        color: Theme.colors.text.disabled
                        font.pixelSize: Theme.secondaryFontSize
                    }
                    Text {
                        Layout.preferredWidth: 44
                        text: "难度"
                        color: Theme.colors.text.disabled
                        font.pixelSize: Theme.secondaryFontSize
                    }
                    Text {
                        Layout.preferredWidth: 44
                        text: "注释"
                        color: Theme.colors.text.disabled
                        font.pixelSize: Theme.secondaryFontSize
                    }
                    Text {
                        Layout.preferredWidth: 64
                        text: "状态"
                        color: Theme.colors.text.disabled
                        font.pixelSize: Theme.secondaryFontSize
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: jobList
                        anchors.fill: parent
                        clip: true
                        model: window.controller.jobs
                        currentIndex: window.controller.selectedRow
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: AppScrollBar {}

                        delegate: Item {
                            id: row
                            required property int index
                            required property string title
                            required property string path
                            required property bool checked
                            required property string status
                            required property string statusText
                            required property int difficultyCount
                            required property int commentCount
                            required property string detail
                            readonly property bool shown: window.matchesFilter(title, path)

                            width: jobList.width
                            height: shown ? 46 : 0
                            visible: shown

                            HoverHandler { id: rowHover }
                            TapHandler { onTapped: window.controller.selectedRow = row.index }

                            HoverChrome {
                                anchors.fill: parent
                                stateColors: Theme.colors.listState
                                selected: row.index === window.controller.selectedRow
                                hovered: rowHover.hovered
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8

                                AppCheckBox {
                                    checkable: false
                                    checked: row.checked
                                    enabled: !window.controller.busy
                                    onClicked: window.controller.setChecked(row.index, !row.checked)
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        Layout.fillWidth: true
                                        text: row.title
                                        elide: Text.ElideRight
                                        color: row.checked ? Theme.colors.text.active : Theme.colors.text.disabled
                                        font.pixelSize: Theme.uiFontSize
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: row.detail.length > 0 ? row.detail : row.path
                                        elide: Text.ElideMiddle
                                        color: Theme.colors.text.disabled
                                        font.pixelSize: Theme.captionFontSize + 1
                                    }
                                }
                                Text {
                                    Layout.preferredWidth: 44
                                    text: row.difficultyCount >= 0 ? row.difficultyCount : "—"
                                    color: Theme.colors.text.secondary
                                }
                                Text {
                                    Layout.preferredWidth: 44
                                    text: row.commentCount >= 0 ? row.commentCount : "—"
                                    color: Theme.colors.text.secondary
                                }
                                Text {
                                    Layout.preferredWidth: 64
                                    text: row.statusText
                                    color: window.statusColor(row.status)
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        visible: window.controller.jobCount === 0
                        spacing: 6
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "拖入 maidata.txt 或曲目文件夹"
                            color: Theme.colors.text.secondary
                            font.pixelSize: Theme.headingFontSize
                        }
                    }
                }
            }

            Hairline { Layout.fillHeight: true }

            // ---- options -----------------------------------------------
            Rectangle {
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                color: Theme.colors.background.panel

                Flickable {
                    anchors.fill: parent
                    contentHeight: optionsColumn.implicitHeight + 32
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true
                    ScrollBar.vertical: AppScrollBar {}

                    ColumnLayout {
                        id: optionsColumn
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 12
                        enabled: !window.controller.busy

                        SettingsSection {
                            Layout.fillWidth: true
                            title: "谱面整理"
                            first: true

                            AppSwitch {
                                text: "整理谱面"
                                checked: window.controller.normalize
                                onToggled: window.controller.normalize = checked
                            }
                            AppSwitch {
                                text: "对齐至 1/384 小节"
                                enabled: window.controller.normalize
                                checked: window.controller.reduceTo384Grid
                                onToggled: window.controller.reduceTo384Grid = checked
                            }
                            OptionCombo {
                                label: "空行分组"
                                enabled: window.controller.normalize
                                model: ["每 4 小节", "每 2 小节", "连续排列"]
                                currentIndex: window.controller.sectionIndex
                                onActivated: (index) => window.controller.sectionIndex = index
                            }
                            OptionCombo {
                                label: "输出格式"
                                enabled: window.controller.normalize
                                model: ["保留分段", "紧凑格式"]
                                currentIndex: window.controller.syntaxIndex
                                onActivated: (index) => window.controller.syntaxIndex = index
                            }
                        }

                        SettingsSection {
                            Layout.fillWidth: true
                            title: "删除注释"

                            AppSwitch {
                                text: "删除谱面中的 || 注释"
                                checked: window.controller.stripComments
                                onToggled: window.controller.stripComments = checked
                            }
                            AppSwitch {
                                text: "删除拍号"
                                enabled: window.controller.stripComments
                                checked: window.controller.stripTimeSignatures
                                onToggled: window.controller.stripTimeSignatures = checked
                            }
                        }

                        SettingsSection {
                            Layout.fillWidth: true
                            title: "统一谱师"

                            AppSwitch {
                                text: "统一谱师名义"
                                checked: window.controller.unifyDesigner
                                onToggled: window.controller.unifyDesigner = checked
                            }
                            AppTextField {
                                id: designerField
                                Layout.fillWidth: true
                                enabled: window.controller.unifyDesigner
                                placeholderText: "谱师名称"
                                text: window.controller.designer
                                onTextEdited: designerCommit.restart()
                                onEditingFinished: {
                                    designerCommit.stop()
                                    window.controller.designer = text
                                }
                            }
                        }
                    }
                }
            }
        }

        Hairline { Layout.fillWidth: true }

        // ---- preview ---------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            color: Theme.colors.background.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "预览"
                        color: Theme.colors.text.section
                        font.pixelSize: Theme.sectionTitleFontSize
                        font.weight: Theme.sectionTitleFontWeight
                    }
                    AppComboBox {
                        compact: true
                        Layout.preferredWidth: 140
                        visible: window.controller.previewSections.length > 0
                        model: window.controller.previewSections
                        currentIndex: window.controller.previewSection
                        onActivated: (index) => window.controller.previewSection = index
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    DiffColumn {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        caption: "原文"
                        rows: window.controller.previewOriginal
                        markColor: window.tint(Theme.colors.danger.primary, 0.2)
                    }
                    DiffColumn {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        caption: "处理后"
                        rows: window.controller.previewCleaned
                        markColor: window.tint(Theme.colors.accent.primary, 0.28)
                    }
                }
            }
        }

        Hairline { Layout.fillWidth: true; visible: window.showMessages }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            visible: window.showMessages && window.controller.messages.length > 0
            color: Theme.colors.background.panel

            ListView {
                id: messageList
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                model: window.controller.messages
                ScrollBar.vertical: AppScrollBar {}
                delegate: Text {
                    required property string modelData
                    width: messageList.width
                    text: modelData
                    wrapMode: Text.Wrap
                    color: Theme.colors.text.secondary
                    font.pixelSize: Theme.secondaryFontSize
                }
            }
        }

        Hairline { Layout.fillWidth: true }

        // ---- footer ----------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 8

            Text {
                text: "输出到"
                color: Theme.colors.text.secondary
            }
            AppTextField {
                Layout.preferredWidth: 320
                enabled: !window.controller.busy
                text: window.controller.outputRoot
                onEditingFinished: {
                    window.controller.outputRoot = text
                    // Editing broke the binding; the controller may have
                    // normalized the path or restored the default.
                    text = Qt.binding(() => window.controller.outputRoot)
                }
            }
            AppButton {
                text: "选择…"
                enabled: !window.controller.busy
                onClicked: outputDialog.open()
            }
            Text {
                Layout.fillWidth: true
                text: window.controller.summary
                elide: Text.ElideRight
                color: Theme.colors.text.secondary
            }
            // Warning-tinted toggle, so it reads as status rather than as one
            // of the actions beside it.
            AbstractButton {
                id: hintsButton
                readonly property color tone: Qt.lighter(Theme.colors.syntax.warning, 1.0)
                visible: window.controller.messages.length > 0
                hoverEnabled: true
                leftPadding: 12
                rightPadding: 12
                implicitHeight: Theme.controlMinHeight
                implicitWidth: Math.max(92, hintsText.implicitWidth + leftPadding + rightPadding)
                onClicked: window.showMessages = !window.showMessages

                contentItem: Text {
                    id: hintsText
                    text: window.controller.messages.length + " 条提示"
                    color: hintsButton.tone
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: HoverChrome {
                    baseColor: window.tint(hintsButton.tone, 0.10)
                    borderColor: window.tint(hintsButton.tone, 0.45)
                    borderWidth: 1
                    stateColors: ({
                        hover: window.tint(hintsButton.tone, 0.18),
                        pressed: window.tint(hintsButton.tone, 0.26),
                        selected: window.tint(hintsButton.tone, 0.22)
                    })
                    selected: window.showMessages
                    hovered: hintsButton.hovered
                    pressed: hintsButton.down
                }
            }
            Rectangle {
                visible: window.controller.busy
                Layout.preferredWidth: 140
                implicitHeight: 4
                radius: 2
                color: Theme.colors.slider.rail
                Rectangle {
                    width: window.controller.progressTotal > 0
                           ? parent.width * window.controller.progress / window.controller.progressTotal : 0
                    height: parent.height
                    radius: 2
                    color: Theme.colors.slider.fill
                }
            }
            Text {
                visible: window.controller.busy
                text: window.controller.progress + " / " + window.controller.progressTotal
                color: Theme.colors.text.secondary
            }
            AppButton {
                visible: !window.controller.busy && window.lastRunWrote
                text: "打开输出目录"
                onClicked: window.controller.openOutputRoot()
            }
            AppButton {
                text: "预检"
                visible: !window.controller.busy
                enabled: window.controller.checkedCount > 0
                onClicked: window.controller.preflight()
            }
            AppButton {
                text: window.controller.busy ? "取消" : "开始处理"
                emphasized: !window.controller.busy
                enabled: window.controller.busy || window.controller.checkedCount > 0
                onClicked: window.controller.busy ? window.controller.cancel() : window.controller.run()
            }
        }
    }

    DropArea {
        id: dropArea
        anchors.fill: parent
        enabled: !window.controller.busy
        onDropped: (drop) => {
            if (drop.hasUrls) {
                window.controller.addUrls(drop.urls)
                drop.acceptProposedAction()
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            visible: dropArea.containsDrag
            color: window.tint(Theme.colors.accent.primary, 0.12)
            border.color: Theme.colors.accent.primary
            border.width: 2
            radius: Theme.workspaceRadius
        }
    }
}
