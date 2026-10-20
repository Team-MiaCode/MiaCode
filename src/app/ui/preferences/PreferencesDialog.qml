import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

AppDialog {
    id: root

    required property var preferencesModel
    required property var shortcuts
    required property var preferences
    required property var appBackground
    property var updateService: null

    signal updateRequested()
    signal completionSettingsClosed()
    property string requestedCompletionGroup: ""
    property bool returnToEditor: false

    function openCompletionSettings(group) {
        root.requestedCompletionGroup = group
        root.returnToEditor = true
        root.open()
    }
    function setEditorScrollPosition(position) {
        const flickable = editorViewport.scrollViewport
        if (!flickable) return
        const minimum = flickable.originY
        const maximum = minimum + Math.max(0, flickable.contentHeight - flickable.height)
        flickable.contentY = Math.max(minimum, Math.min(position, maximum))
    }
    function clampEditorScroll() {
        const flickable = editorViewport.scrollViewport
        if (flickable) root.setEditorScrollPosition(flickable.contentY)
    }
    function scrollCompletionBy(distance) {
        root.setEditorScrollPosition(editorViewport.scrollViewport.contentY + distance)
    }
    function releaseCompletionInputFocus() {
        completionCustomizer.releaseInputFocus()
    }
    function ensureCompletionItemVisible(item, alignment) {
        if (alignment === "top") {
            completionCustomizer.ensureContentLayout()
            editorSettingsColumn.ensurePolished()
            editorViewport.ensurePolished()
            editorViewport.scrollViewport.ensurePolished()
        }
        const flickable = editorViewport.scrollViewport
        const position = item.mapToItem(flickable.contentItem, 0, 0)
        const visibleHeight = flickable.height
        let target = flickable.contentY
        if (alignment === "top") {
            editorViewport.completionAlignmentY = position.y
            editorViewport.completionTopAligned = true
            editorViewport.ensurePolished()
            flickable.ensurePolished()
            target = position.y
        } else {
            const itemHeight = Math.min(item.height, visibleHeight)
            if (position.y < target) target = position.y
            else if (position.y + itemHeight > target + visibleHeight)
                target = position.y + itemHeight - visibleHeight
        }
        root.setEditorScrollPosition(target)
    }
    onOpened: {
        if (root.requestedCompletionGroup.length > 0) {
            root.currentPageIndex = root.pageIds.indexOf("editor")
            completionCustomizer.openGroup(root.requestedCompletionGroup)
        }
    }
    onAboutToHide: {
        root.releaseCompletionInputFocus()
        root.preferencesModel.finishCompletionEditing()
    }
    onClosed: {
        root.requestedCompletionGroup = ""
        if (root.returnToEditor) {
            root.returnToEditor = false
            root.completionSettingsClosed()
        }
    }

    title: qsTrId("dialog.preferences.title")
    preferredWidth: 960
    preferredHeight: Theme.dialogHeight + 140
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    header: null
    footer: null

    property int currentPageIndex: 0
    property string capturingId: ""

    Binding {
        target: root.shortcuts
        property: "captureActive"
        value: root.visible && root.capturingId.length > 0
    }
    property var shortcutRows: []
    readonly property var aboutInfo: root.preferences.aboutInfo()
    readonly property real sidebarWidth: root.width < 650 ? 140 : 180
    readonly property int settingsRowHeight: Theme.controlMinHeight + 10
    readonly property int settingsLabelWidth: root.width < 650 ? 120 : 140
    readonly property int sectionSpacing: 2 * Theme.settingsRowSpacing
    readonly property var pageIds: ["interface", "editor", "background", "performance", "shortcuts", "about"]
    readonly property var pageTitles: [
        qsTrId("qml.interface"), qsTrId("dialog.preferences.editor_group"),
        qsTrId("dialog.preferences.background_group"), qsTrId("dialog.preferences.performance_group"),
        qsTrId("dialog.preferences.shortcuts_group"),
        qsTrId("action.about")
    ]

    onCurrentPageIndexChanged: {
        root.capturingId = ""
        root.releaseCompletionInputFocus()
        if (root.currentPageIndex !== root.pageIds.indexOf("editor"))
            completionCustomizer.expanded = false
    }
    onAboutToShow: {
        root.capturingId = ""
        completionCustomizer.expanded = false
        root.refreshShortcuts()
    }

    function openPage(pageId) {
        root.currentPageIndex = root.pageIds.indexOf(pageId)
        root.open()
    }

    function focusPage(index) {
        root.currentPageIndex = (index + root.pageIds.length) % root.pageIds.length
        const button = root.currentPageIndex === root.pageIds.length - 1
            ? aboutButton : navigation.itemAt(root.currentPageIndex)
        button.forceActiveFocus(Qt.TabFocusReason)
    }

    function shortcutGroup(id) {
        switch (id.split(".")[0]) {
        case "ui": return qsTrId("qml.interface")
        case "transform": return qsTrId("dialog.preferences.group.chart_transforms")
        case "preview": return qsTrId("dialog.preferences.group.playback")
        case "timeline": return qsTrId("dialog.preferences.group.timeline")
        case "editor": return qsTrId("dialog.preferences.editor_group")
        default: return qsTrId("dialog.preferences.shortcuts_group")
        }
    }

    function refreshShortcuts() {
        const keepY = shortcutList.contentY
        root.shortcutRows = root.shortcuts.editableShortcuts().map(row =>
            Object.assign({group: root.shortcutGroup(row.id)}, row))
        shortcutList.contentY = Math.max(
            0, Math.min(keepY, Math.max(0, shortcutList.contentHeight - shortcutList.height)))
    }

    component PreferenceGroup: SettingsSection {
        Layout.fillWidth: true
        showDivider: false
        titleFontSize: Theme.uiFontSize + 1
        spacing: 12
        rowSpacing: 6
    }

    component PreferencePage: ScrollView {
        id: page
        default property alias fields: fieldColumn.data
        contentWidth: availableWidth
        rightPadding: pageScrollBar.width + 4
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: AppScrollBar {
            id: pageScrollBar
            parent: page
            x: page.width - width
            y: page.topPadding
            height: page.availableHeight
        }

        ColumnLayout {
            id: fieldColumn
            width: page.availableWidth
            spacing: root.sectionSpacing
        }
    }

    component PreferenceCombo: LabeledCombo {
        Layout.minimumHeight: root.settingsRowHeight
        labelWidth: root.settingsLabelWidth
    }

    component PreferenceSlider: LabeledSlider {
        Layout.minimumHeight: root.settingsRowHeight
        labelWidth: root.settingsLabelWidth
    }

    component PreferenceSwitch: AppSwitch {
        Layout.fillWidth: true
        Layout.minimumHeight: root.settingsRowHeight
        indicatorOnRight: true
    }

    component NavigationButton: ChromeRow {
        id: navigationButton
        required property int pageIndex
        Layout.fillWidth: true
        implicitHeight: root.settingsRowHeight
        selected: root.currentPageIndex === navigationButton.pageIndex
        text: root.pageTitles[navigationButton.pageIndex]
        labelFont: Qt.font({
            family: Theme.uiFont,
            pixelSize: Theme.uiFontSize,
            weight: navigationButton.selected ? Font.DemiBold : Font.Normal
        })
        Accessible.role: Accessible.PageTab
        Accessible.name: navigationButton.text
        Accessible.selected: navigationButton.selected
        onClicked: root.currentPageIndex = navigationButton.pageIndex
        Keys.onUpPressed: root.focusPage(navigationButton.pageIndex - 1)
        Keys.onDownPressed: root.focusPage(navigationButton.pageIndex + 1)
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Home) {
                root.focusPage(0)
                event.accepted = true
            } else if (event.key === Qt.Key_End) {
                root.focusPage(root.pageIds.length - 1)
                event.accepted = true
            }
        }
    }

    component AboutInfoLabel: Text {
        Layout.preferredWidth: root.settingsLabelWidth
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
    }

    component AboutInfoValue: TextEdit {
        Layout.fillWidth: true
        readOnly: true
        selectByMouse: true
        color: Theme.colors.text.primary
        selectionColor: Theme.colors.state.textSelection
        selectedTextColor: Theme.colors.text.active
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        wrapMode: TextEdit.Wrap
    }

    background: FloatingCard {
        id: dialogSurface
        popup: root
        tintColor: Theme.dialogTintColor
        blurRadius: Theme.dialogBlurRadius
        shadowBlur: 0.68
        shadowOpacity: Theme.dialogShadowOpacity
        shadowVerticalOffset: 3
        tintDelegate: Item {
            readonly property real tintOpacity: dialogSurface.materialActive ? Theme.dialogTintOpacity : 1
            readonly property color sidebarTint: Theme.colors.background.surface
            readonly property color panelTint: Theme.colors.background.panel

            Rectangle {
                width: root.sidebarWidth
                height: parent.height
                topLeftRadius: Theme.popupRadius
                bottomLeftRadius: Theme.popupRadius
                color: Qt.rgba(parent.sidebarTint.r, parent.sidebarTint.g,
                               parent.sidebarTint.b, parent.tintOpacity)
            }
            Rectangle {
                x: root.sidebarWidth
                width: parent.width - x
                height: parent.height
                topRightRadius: Theme.popupRadius
                bottomRightRadius: Theme.popupRadius
                color: Qt.rgba(parent.panelTint.r, parent.panelTint.g,
                               parent.panelTint.b, parent.tintOpacity)
            }
        }
    }

    contentItem: RowLayout {
        id: preferencesBody
        spacing: 0

        TapHandler {
            parent: preferencesBody
            enabled: root.visible
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            gesturePolicy: TapHandler.DragThreshold
            onPressedChanged: {
                if (pressed)
                    completionCustomizer.releaseInputFocusOutside(preferencesBody, point.position)
            }
        }

        Item {
            Layout.preferredWidth: root.sidebarWidth
            Layout.fillHeight: true

            Rectangle {
                anchors.right: parent.right
                width: 1 / Screen.devicePixelRatio
                height: parent.height
                color: Theme.separatorColor
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 24
                anchors.bottomMargin: 24
                spacing: 4

                Repeater {
                    id: navigation
                    model: root.pageIds.length - 1
                    delegate: NavigationButton {
                        required property int index
                        pageIndex: index
                    }
                }
                Item { Layout.fillHeight: true }
                NavigationButton {
                    id: aboutButton
                    objectName: "preferencesAboutButton"
                    pageIndex: root.pageIds.length - 1
                }
                Text {
                    objectName: "preferencesVersion"
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.rowPaddingX
                    Layout.rightMargin: Theme.rowPaddingX
                    Layout.topMargin: -3
                    text: qsTrId("about.version").arg(root.aboutInfo.version)
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.captionFontSize
                    color: Theme.colors.text.secondary
                    opacity: 0.6
                    wrapMode: Text.WordWrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root.width < 650 ? 18 : 32
            Layout.rightMargin: root.width < 650 ? 18 : 32
            Layout.topMargin: 24
            Layout.bottomMargin: 24
            spacing: root.sectionSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.settingsRowSpacing

                Text {
                    Layout.fillWidth: true
                    text: root.pageTitles[root.currentPageIndex]
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize + 8
                    font.weight: Font.DemiBold
                    color: Theme.colors.text.heading
                    elide: Text.ElideRight
                }
                IconButton {
                    objectName: "preferencesCloseButton"
                    iconSource: Qt.resolvedUrl("icons/close.svg")
                    tooltip: qsTrId("action.close")
                    onClicked: root.reject()
                }
            }

            AppTabPages {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPageIndex

                PreferencePage {
                    PreferenceGroup {
                        title: qsTrId("dialog.preferences.group.appearance")
                        first: true

                        PreferenceCombo {
                            objectName: "preferencesThemeModeCombo"
                            label: qsTrId("dialog.preferences.theme.mode")
                            options: root.preferencesModel.themeModeOptions
                            currentValue: root.preferencesModel.themeModeToken
                            onPicked: function(value) { root.preferencesModel.themeModeToken = value }
                        }

                        PreferenceCombo {
                            objectName: "preferencesLightThemeCombo"
                            visible: root.preferencesModel.themeModeToken !== "dark"
                            label: qsTrId("dialog.preferences.theme.light_palette")
                            options: root.preferencesModel.themePaletteOptions
                            currentValue: root.preferencesModel.lightThemeToken
                            onPicked: function(value) { root.preferencesModel.lightThemeToken = value }
                        }

                        PreferenceCombo {
                            objectName: "preferencesDarkThemeCombo"
                            visible: root.preferencesModel.themeModeToken !== "light"
                            label: qsTrId("dialog.preferences.theme.dark_palette")
                            options: root.preferencesModel.themePaletteOptions
                            currentValue: root.preferencesModel.darkThemeToken
                            onPicked: function(value) { root.preferencesModel.darkThemeToken = value }
                        }

                    }

                    PreferenceGroup {
                        title: qsTrId("dialog.preferences.group.workspace")

                        PreferenceCombo {
                            objectName: "preferencesLanguageCombo"
                            label: qsTrId("dialog.preferences.language")
                            options: root.preferencesModel.languageOptions
                            currentValue: root.preferencesModel.languageToken
                            onPicked: function(value) {
                                root.preferencesModel.languageToken = value
                                root.refreshShortcuts()
                            }
                        }

                        PreferenceCombo {
                            objectName: "preferencesSidebarModeCombo"
                            label: qsTrId("dialog.preferences.sidebar_mode")
                            options: root.preferencesModel.sidebarModeOptions
                            currentValue: root.preferences.efficientSidebarEnabled
                            onPicked: value => root.preferences.efficientSidebarEnabled = value
                        }

                        PreferenceCombo {
                            objectName: "preferencesUiScaleCombo"
                            label: qsTrId("dialog.preferences.ui_scale")
                            options: root.preferencesModel.uiScaleOptions
                            currentValue: root.preferencesModel.uiScalePercent
                            onPicked: value => root.preferencesModel.uiScalePercent = value
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5

                            Text {
                                Layout.preferredWidth: root.settingsLabelWidth
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: Math.round((Theme.controlMinHeight - implicitHeight) / 2)
                                text: qsTrId("dialog.preferences.interface_layout")
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: Theme.chromeInsetY
                                spacing: 12

                                PreviewPositionCard {
                                    objectName: "preferencesPreviewRightCard"
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 200
                                    previewOnLeft: false
                                    selected: !root.preferencesModel.previewOnLeft
                                    onClicked: root.preferencesModel.previewOnLeft = false
                                }
                                PreviewPositionCard {
                                    objectName: "preferencesPreviewLeftCard"
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 200
                                    previewOnLeft: true
                                    selected: root.preferencesModel.previewOnLeft
                                    onClicked: root.preferencesModel.previewOnLeft = true
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }

                ScrollView {
                    id: editorViewport
                    readonly property Flickable scrollViewport: contentItem as Flickable
                    property bool completionTopAligned: false
                    property real completionAlignmentY: 0
                    readonly property real scrollContentSpacing: 4
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    implicitHeight: 0
                    clip: true
                    rightPadding: editorScrollBar.width + scrollContentSpacing
                    contentWidth: availableWidth
                    onAvailableHeightChanged: Qt.callLater(root.clampEditorScroll)
                    contentHeight: Math.max(editorSettingsColumn.implicitHeight,
                        completionTopAligned ? completionAlignmentY + availableHeight : 0)
                    onContentHeightChanged: Qt.callLater(root.clampEditorScroll)
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical: AppScrollBar {
                        id: editorScrollBar
                        parent: editorViewport
                        orientation: Qt.Vertical
                        x: editorViewport.width - width
                        y: editorViewport.topPadding
                        height: editorViewport.availableHeight
                        policy: ScrollBar.AsNeeded
                        onPressedChanged: if (pressed) root.releaseCompletionInputFocus()
                    }

                    TapHandler {
                        parent: editorViewport
                        enabled: root.visible
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                        gesturePolicy: TapHandler.DragThreshold
                        onPressedChanged: {
                            if (pressed)
                                completionCustomizer.releaseInputFocusOutside(editorViewport, point.position)
                        }
                    }

                    Connections {
                        target: editorViewport.scrollViewport
                        function onMovingChanged() {
                            if (editorViewport.scrollViewport.moving) root.releaseCompletionInputFocus()
                        }
                    }

                    ColumnLayout {
                        id: editorSettingsColumn
                        width: editorViewport.availableWidth
                        spacing: root.sectionSpacing

                        PreferenceGroup {
                            title: qsTrId("dialog.preferences.group.typography")
                            first: true

                            PreferenceSlider {
                                objectName: "preferencesFontSizeSlider"
                                label: qsTrId("dialog.preferences.editor_font_size")
                                from: root.preferencesModel.editorFontSizeMinimum
                                to: root.preferencesModel.editorFontSizeMaximum
                                value: root.preferencesModel.editorFontSize
                                readout: root.preferencesModel.editorFontSize + " pt"
                                onMoved: function(v) { root.preferencesModel.editorFontSize = Math.round(v) }
                            }

                            PreferenceCombo {
                                objectName: "preferencesLineSpacingCombo"
                                label: qsTrId("dialog.preferences.editor_line_spacing")
                                options: root.preferencesModel.lineSpacingOptions
                                currentValue: root.preferencesModel.editorLineSpacing
                                onPicked: function(value) { root.preferencesModel.editorLineSpacing = value }
                            }

                            PreferenceSwitch {
                                objectName: "preferencesAutoWrapSwitch"
                                text: qsTrId("preferences.editor_auto_wrap")
                                checked: root.preferencesModel.editorAutoWrap
                                onToggled: root.preferencesModel.editorAutoWrap = checked
                            }
                        }

                        PreferenceGroup {
                            title: qsTrId("dialog.preferences.group.input")

                            PreferenceCombo {
                                objectName: "preferencesInputHandlingCombo"
                                label: qsTrId("preferences.input_handling")
                                options: root.preferencesModel.inputHandlingOptions
                                currentValue: root.preferencesModel.editorInputHandlingMode
                                onPicked: function(value) { root.preferencesModel.editorInputHandlingMode = value }
                            }

                            PreferenceSwitch {
                                objectName: "preferencesAutoCompletionSwitch"
                                text: qsTrId("preferences.auto_completion")
                                checked: root.preferencesModel.editorAutoCompletion
                                onToggled: root.preferencesModel.editorAutoCompletion = checked
                            }

                            CompletionCandidatesEditor {
                                id: completionCustomizer
                                Layout.fillWidth: true
                                visible: root.preferencesModel.editorAutoCompletion
                                preferencesModel: root.preferencesModel
                                scrollViewport: editorViewport.scrollViewport
                                onExpandedChanged: {
                                    if (!completionCustomizer.expanded)
                                        editorViewport.completionTopAligned = false
                                }
                                onEnsureVisibleRequested: function(item, alignment) {
                                    root.ensureCompletionItemVisible(item, alignment)
                                }
                                onScrollRequested: function(distance) { root.scrollCompletionBy(distance) }
                            }
                        }

                        PreferenceGroup {
                            title: qsTrId("dialog.preferences.group.editing_aids")

                            PreferenceSwitch {
                                objectName: "preferencesScrollPastEndSwitch"
                                text: qsTrId("preferences.editor_scroll_past_end")
                                checked: root.preferencesModel.editorScrollPastEnd
                                onToggled: root.preferencesModel.editorScrollPastEnd = checked
                            }

                            PreferenceSwitch {
                                objectName: "preferencesSelectionBeatDisplaySwitch"
                                text: qsTrId("preferences.editor_selection_beat_display")
                                checked: root.preferencesModel.editorSelectionBeatDisplay
                                onToggled: root.preferencesModel.editorSelectionBeatDisplay = checked
                            }

                            PreferenceSwitch {
                                objectName: "preferencesOverviewRulerSwitch"
                                text: qsTrId("preferences.editor_overview_ruler")
                                checked: root.preferencesModel.editorOverviewRulerEnabled
                                onToggled: root.preferencesModel.editorOverviewRulerEnabled = checked
                            }
                        }
                    }
                }

                PreferencePage {
                    objectName: "preferencesBackgroundPage"
                    PreferenceGroup {
                        title: qsTrId("dialog.preferences.group.background_image")
                        first: true

                        PreferenceSwitch {
                            text: qsTrId("qml.enable_application_background")
                            checked: root.appBackground.enabled
                            onToggled: root.appBackground.enabled = checked
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.settingsRowHeight
                            spacing: 8
                            Text {
                                font.pixelSize: Theme.uiFontSize
                                Layout.fillWidth: true
                                text: root.appBackground.imagePath.length > 0
                                      ? root.appBackground.imagePath
                                      : qsTrId("qml.no_background_image_selected")
                                color: root.appBackground.imageReadable
                                       ? Theme.colors.text.primary : Theme.colors.text.secondary
                                elide: Text.ElideMiddle
                                font.family: Theme.uiFont
                            }
                            AppButton {
                                text: qsTrId("cover.choose_image")
                                onClicked: root.appBackground.chooseImage()
                            }
                            AppButton {
                                text: qsTrId("dialog.preferences.background.clear")
                                enabled: root.appBackground.imagePath.length > 0
                                onClicked: root.appBackground.clearImage()
                            }
                        }

                        Text {
                            font.pixelSize: Theme.uiFontSize
                            Layout.fillWidth: true
                            visible: root.appBackground.errorMessage.length > 0
                            text: root.appBackground.errorMessage
                            color: Theme.colors.syntax.error
                            font.family: Theme.uiFont
                            wrapMode: Text.WordWrap
                        }

                        PreferenceSlider {
                            label: qsTrId("qml.image_opacity")
                            from: 0.1; to: 0.8; stepSize: 0.01
                            value: root.appBackground.opacity
                            readout: Math.round(root.appBackground.opacity * 100) + "%"
                            onMoved: function(value) { root.appBackground.opacity = value }
                        }
                    }

                    PreferenceGroup {
                        title: qsTrId("dialog.preferences.group.presentation")

                        PreferenceCombo {
                            label: qsTrId("qml.scale_mode")
                            options: [
                                { value: "cover", label: qsTrId("dialog.preferences.background.scale.cover") },
                                { value: "contain", label: qsTrId("dialog.preferences.background.scale.contain") },
                                { value: "stretch", label: qsTrId("dialog.preferences.background.scale.stretch") },
                                { value: "center", label: qsTrId("dialog.preferences.background.scale.center") },
                                { value: "repeat", label: qsTrId("dialog.preferences.background.scale.repeat") }
                            ]
                            currentValue: root.appBackground.sizeMode
                            onPicked: function(value) { root.appBackground.sizeMode = value }
                        }

                        PreferenceCombo {
                            label: qsTrId("dialog.preferences.background.position")
                            options: [
                                { value: "center", label: qsTrId("dialog.preferences.background.position.center") },
                                { value: "left", label: qsTrId("dialog.preferences.background.position.left") },
                                { value: "right", label: qsTrId("dialog.preferences.background.position.right") },
                                { value: "top", label: qsTrId("dialog.preferences.background.position.top") },
                                { value: "bottom", label: qsTrId("dialog.preferences.background.position.bottom") },
                                { value: "left_top", label: qsTrId("dialog.preferences.background.position.left_top") },
                                { value: "right_top", label: qsTrId("dialog.preferences.background.position.right_top") },
                                { value: "left_bottom", label: qsTrId("dialog.preferences.background.position.left_bottom") },
                                { value: "right_bottom", label: qsTrId("dialog.preferences.background.position.right_bottom") }
                            ]
                            currentValue: root.appBackground.position
                            onPicked: function(value) { root.appBackground.position = value }
                        }

                        PreferenceSlider {
                            label: qsTrId("qml.background_mask_opacity")
                            from: 0; to: 1; stepSize: 0.01
                            value: root.appBackground.panelAlpha / 255.0
                            readout: Math.round(root.appBackground.panelAlpha / 255.0 * 100) + "%"
                            onMoved: function(value) { root.appBackground.panelAlpha = Math.round(value * 255) }
                        }

                        PreferenceSlider {
                            label: qsTrId("qml.blur_radius")
                            from: 0; to: 64; stepSize: 1
                            value: root.appBackground.blur
                            readout: Math.round(root.appBackground.blur)
                            onMoved: function(value) { root.appBackground.blur = Math.round(value) }
                        }
                    }
                }

                PreferencePage {
                    PreferenceGroup {
                        title: qsTrId("dialog.preferences.group.video")
                        first: true

                        PreferenceCombo {
                            objectName: "preferencesVideoDecodeCombo"
                            label: qsTrId("qml.video_decoding")
                            options: [{ value: false, label: qsTrId("qml.hardware_decoding") }, { value: true, label: qsTrId("qml.software_decoding") }]
                            currentValue: root.preferencesModel.videoDecodePrefersSoftware
                            onPicked: function(value) { root.preferencesModel.videoDecodePrefersSoftware = value }
                        }

                        PreferenceCombo {
                            objectName: "preferencesPvFrameRateCombo"
                            label: qsTrId("qml.pv_frame_rate")
                            options: root.preferencesModel.appFrameRateOptions
                            currentValue: root.preferencesModel.stageMediaFrameRateMode
                            onPicked: function(value) { root.preferencesModel.stageMediaFrameRateMode = value }
                        }
                    }

                    PreferenceGroup {
                        title: qsTrId("dialog.preferences.group.rendering")

                        PreferenceSwitch {
                            objectName: "preferencesBlurMaterialsSwitch"
                            text: qsTrId("dialog.preferences.blur_materials")
                            checked: root.preferencesModel.blurMaterialsEnabled
                            onToggled: root.preferencesModel.blurMaterialsEnabled = checked
                        }

                        PreferenceCombo {
                            objectName: "preferencesCanvasFrameRateCombo"
                            label: qsTrId("qml.canvas_frame_rate")
                            options: root.preferencesModel.canvasFrameRateOptions
                            currentValue: root.preferencesModel.canvasFrameRateMode
                            onPicked: function(value) { root.preferencesModel.canvasFrameRateMode = value }
                        }

                        PreferenceCombo {
                            objectName: "preferencesTimelineFrameRateCombo"
                            label: qsTrId("qml.timeline_frame_rate")
                            options: root.preferencesModel.appFrameRateOptions
                            currentValue: root.preferencesModel.timelineFrameRateMode
                            onPicked: function(value) { root.preferencesModel.timelineFrameRateMode = value }
                        }
                    }
                }

                ColumnLayout {
                    spacing: root.sectionSpacing

                    ListView {
                        id: shortcutList
                        objectName: "shortcutList"
                        property bool reservesPlainSpace: root.capturingId.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        focus: true
                        spacing: 6
                        model: root.shortcutRows
                        section.property: "group"
                        section.criteria: ViewSection.FullString
                        section.delegate: Column {
                            required property string section
                            readonly property bool first: root.shortcutRows.length > 0
                                                          && section === root.shortcutRows[0].group
                            width: ListView.view.width
                            spacing: 0

                            Item { width: 1; height: parent.first ? 0 : root.sectionSpacing }
                            Text {
                                text: parent.section
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize + 1
                                font.weight: Font.DemiBold
                                color: Theme.colors.text.section
                            }
                            Item { width: 1; height: 12 }
                        }
                        ScrollBar.vertical: AppScrollBar {}

                        Keys.onPressed: function(event) {
                            if (root.capturingId.length === 0)
                                return
                            event.accepted = true
                            if (event.key === Qt.Key_Escape) {
                                root.capturingId = ""
                                return
                            }
                            if (event.key === Qt.Key_Control || event.key === Qt.Key_Shift
                                    || event.key === Qt.Key_Alt || event.key === Qt.Key_Meta)
                                return
                            const text = root.shortcuts.shortcutTextForKeyEvent(event.key, event.modifiers)
                            if (text.length === 0)
                                return
                            root.shortcuts.setShortcutText(root.capturingId, text)
                            root.capturingId = ""
                            root.refreshShortcuts()
                        }

                        // The row is NOT one big button: the reset button sits beside
                        // the clickable area, not inside it, so the highlight stops
                        // where the recording hit area stops.
                        delegate: RowLayout {
                            id: shortcutRow
                            required property var modelData
                            width: ListView.view.width
                            spacing: 8

                            ChromeRow {
                                id: captureArea
                                Layout.fillWidth: true
                                implicitHeight: root.settingsRowHeight
                                selected: root.capturingId === shortcutRow.modelData.id
                                onClicked: {
                                    root.capturingId = shortcutRow.modelData.id
                                    shortcutList.forceActiveFocus()
                                }
                                contentItem: RowLayout {
                                    spacing: 8
                                    Text {
                                        font.pixelSize: Theme.uiFontSize
                                        Layout.fillWidth: true
                                        text: qsTrId(shortcutRow.modelData.labelKey)
                                              || shortcutRow.modelData.labelFallback
                                        elide: Text.ElideRight
                                        color: Theme.colors.text.active
                                        font.family: Theme.uiFont
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    Text {
                                        font.pixelSize: Theme.uiFontSize
                                        Layout.preferredWidth: 170
                                        text: root.capturingId === shortcutRow.modelData.id
                                              ? qsTrId("qml.recording")
                                              : shortcutRow.modelData.shortcutText
                                        color: shortcutRow.modelData.isDefault
                                               ? Theme.colors.text.secondary
                                               : Theme.colors.accent.primary
                                        font.family: Theme.uiFont
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            AppButton {
                                text: qsTrId("qml.restore_defaults")
                                enabled: !shortcutRow.modelData.isDefault
                                onClicked: {
                                    root.shortcuts.resetShortcut(shortcutRow.modelData.id)
                                    root.refreshShortcuts()
                                }
                            }
                        }
                    }

                    AppButton {
                        objectName: "preferencesResetShortcutsButton"
                        Layout.alignment: Qt.AlignRight
                        text: qsTrId("qml.restore_all_defaults")
                        onClicked: {
                            root.shortcuts.resetAllShortcuts()
                            root.refreshShortcuts()
                        }
                    }
                }

                PreferencePage {
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Image {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            source: "qrc:/icons/app.png"
                            sourceSize: Qt.size(96, 96)
                            fillMode: Image.PreserveAspectFit
                        }

                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: "MiaCode"
                                color: Theme.colors.text.active
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize + 6
                                font.bold: true
                            }
                            Text {
                                objectName: "aboutVersion"
                                text: "v" + root.aboutInfo.version
                                color: Theme.colors.text.secondary
                            }
                            Text {
                                objectName: "aboutUpdateHint"
                                visible: root.updateService && root.updateService.updateAvailable
                                text: qsTrId("status.update_available").arg(
                                    root.updateService ? root.updateService.availableVersion : "")
                                color: Theme.colors.text.active
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.secondaryFontSize

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.updateRequested()
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: Theme.settingsLabelSpacing
                        rowSpacing: 12

                        AboutInfoLabel { text: root.aboutInfo.platformLabel }
                        // Selectable: these are the two lines a bug report asks for.
                        AboutInfoValue { text: root.aboutInfo.platform }
                        AboutInfoLabel { text: root.aboutInfo.buildTypeLabel }
                        AboutInfoValue { text: root.aboutInfo.buildType }
                    }

                    PreferenceGroup {
                        title: qsTrId("dialog.preferences.updates_group")

                        PreferenceSwitch {
                            objectName: "preferencesUpdateCheckSwitch"
                            text: qsTrId("dialog.preferences.update.auto_check")
                            checked: root.updateService ? root.updateService.checkEnabled : false
                            onToggled: if (root.updateService) root.updateService.checkEnabled = checked
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.updateService && root.updateService.lastCheckText.length > 0
                            text: qsTrId("dialog.preferences.update.last_check").arg(
                                root.updateService ? root.updateService.lastCheckText : "")
                            color: Theme.colors.text.secondary
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.secondaryFontSize
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.settingsRowHeight
                            spacing: 12

                            Text {
                                id: updateManualResult
                                property string outcome: ""
                                text: {
                                    if (outcome === "up-to-date") return qsTrId("dialog.preferences.update.up_to_date")
                                    if (outcome === "no-package") return qsTrId("dialog.preferences.update.no_package")
                                    if (outcome === "" || outcome === "available") return ""
                                    return qsTrId("dialog.preferences.update.failed")
                                }
                                objectName: "preferencesUpdateManualResultText"
                                Layout.fillWidth: true
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.secondaryFontSize
                                wrapMode: Text.WordWrap
                            }

                            AppButton {
                                objectName: "preferencesUpdateCheckNowButton"
                                text: qsTrId("dialog.preferences.update.check_now")
                                enabled: root.updateService && !root.updateService.checkInFlight
                                onClicked: if (root.updateService) root.updateService.checkNow(true)
                            }
                        }

                        Connections {
                            target: root.updateService
                            function onManualCheckFinished(outcome, detail) {
                                updateManualResult.outcome = outcome
                                if (outcome === "available") {
                                    root.updateRequested()
                                    return
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
