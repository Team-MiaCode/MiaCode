pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Dialogs
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    signal chartRequested(url chart, int preferredDifficultyId)
    signal chartFolderRequested(url chart)
    required property string activeChartPath
    property bool compact: false
    property bool efficientMode: false
    property bool nativeMaterialActive: false
    readonly property alias cornerSourceItem: heading
    readonly property bool hasDirectory: library.directory.toString().length > 0
    readonly property bool popupActive: directoryPicker.visible || difficultyMenu.active || songMenu.active
    readonly property real contentMargin: 10
    readonly property int toolbarButtonWidth: root.efficientMode ? Theme.chromeMinSize : Theme.controlHighlightHeight
    readonly property int toolbarIconSize: root.efficientMode ? 15 : 16
    readonly property var rowStateColors: nativeMaterialActive ? ({
        hover: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05),
        pressed: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(0, 0, 0, 0.09),
        selected: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(0, 0, 0, 0.08)
    }) : root.compact ? Theme.colors.popupState : Theme.colors.listState
    readonly property var buttonStateColors: nativeMaterialActive ? ({
        hover: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06),
        pressed: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.11),
        selected: Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.10)
    }) : root.compact ? Theme.colors.popupState : Theme.colors.buttonState
    readonly property string activeChartKey: library.chartPathKey(activeChartPath)
    property bool loaded: false
    property list<int> selectedDifficulties: []
    readonly property var difficultyOptions: [qsTrId("library.difficulty.1"), qsTrId("library.difficulty.2"),
        qsTrId("library.difficulty.3"), qsTrId("library.difficulty.4"),
        qsTrId("library.difficulty.5"), qsTrId("library.difficulty.6"),
        qsTrId("library.difficulty.7")]
    readonly property var filteredSongs: {
        const query = search.text.trim().toLocaleLowerCase()
        const difficulties = root.selectedDifficulties
        return library.songs.filter(song =>
            (difficulties.length === 0 || song.difficulties.some(entry => difficulties.includes(entry.id)))
            && (query.length === 0 || [song.title, song.artist, song.variant, song.songId]
                .some(value => value?.toLocaleLowerCase().includes(query))))
    }

    onSelectedDifficultiesChanged: resetSongPosition()

    function setDifficultySelected(difficultyId: int, selected: bool) {
        selectedDifficulties = selected
            ? [...selectedDifficulties, difficultyId]
            : selectedDifficulties.filter(id => id !== difficultyId)
    }

    function resetSongPosition() {
        viewport.songIndex = -1
        viewport.positionViewAtBeginning()
    }

    function focusSong(index) {
        if (index < 0 || index >= viewport.count)
            return false

        viewport.songIndex = index
        viewport.positionViewAtIndex(index, ListView.Contain)
        viewport.forceActiveFocus()
        return true
    }

    function openSongAt(index, preferredDifficultyId) {
        const song = filteredSongs[index]
        if (!song)
            return
        viewport.songIndex = index
        const matchingDifficulty = song.difficulties.find(entry => selectedDifficulties.includes(entry.id))
        root.chartRequested(song.chart, preferredDifficultyId ?? matchingDifficulty?.id ?? 0)
    }

    function openCurrentResult() {
        const index = Math.max(0, Math.min(viewport.songIndex, filteredSongs.length - 1))
        openSongAt(index)
    }

    function showSongMenu(row, x, y) {
        const point = row.mapToItem(root, x, y)
        songMenu.chart = row.modelData.chart
        songMenu.popup(root, point.x, point.y)
    }

    function openDirectoryPicker() {
        const folder = library.pickerDirectory(root.activeChartPath)
        if (folder.toString().length > 0)
            directoryPicker.currentFolder = folder
        directoryPicker.open()
    }

    onVisibleChanged: {
        if (visible && !loaded && library.directory.toString().length > 0) {
            loaded = true
            library.refresh()
        }
    }

    ChartLibraryModel {
        id: library
    }

    AppMenu {
        id: songMenu
        objectName: "chartLibrarySongMenu"
        property url chart
        hugContent: true

        AppMenuItem {
            text: qsTrId("action.open_folder")
            onTriggered: root.chartFolderRequested(songMenu.chart)
        }
    }

    FolderDialog {
        id: directoryPicker
        title: qsTrId("library.choose_folder")
        onAccepted: {
            root.loaded = true
            root.selectedDifficulties = []
            library.setDirectory(selectedFolder)
        }
    }

    component LibraryIconButton: IconButton {
        implicitWidth: root.toolbarButtonWidth
        implicitHeight: Theme.controlMinHeight
        width: root.toolbarButtonWidth
        height: Theme.controlMinHeight
        Layout.preferredWidth: root.toolbarButtonWidth
        Layout.preferredHeight: Theme.controlMinHeight
        iconWidth: root.toolbarIconSize
        iconHeight: root.toolbarIconSize
        cornerRadius: Theme.controlRadius
        backgroundFillsWidth: true
        stateColors: root.buttonStateColors
    }

    PanelHeader {
        id: heading
        objectName: "chartLibraryHeader"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: !root.efficientMode
        height: visible ? implicitHeight : 0
        title: qsTrId("library.title")
        sidebarTitle: true

        LibraryIconButton {
            objectName: "chartLibraryFolderButton"
            visible: !root.efficientMode && root.hasDirectory
            iconSource: Qt.resolvedUrl("icons/folder-open.svg")
            tooltip: qsTrId("library.choose_folder")
            onClicked: root.openDirectoryPicker()
        }
        LibraryIconButton {
            objectName: "chartLibraryRefreshButton"
            visible: !root.efficientMode && root.hasDirectory
            iconSource: Qt.resolvedUrl("icons/refresh.svg")
            tooltip: qsTrId("library.refresh")
            enabled: !library.scanning && root.hasDirectory
            onClicked: library.refresh()
        }
    }

    ColumnLayout {
        objectName: "chartLibraryListColumn"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.topMargin: root.efficientMode ? 2 : 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        spacing: 8

        RowLayout {
            objectName: "chartLibrarySearchRow"
            Layout.fillWidth: true
            Layout.leftMargin: root.contentMargin
            Layout.rightMargin: root.contentMargin
            visible: root.hasDirectory
            spacing: 6

            AppTextField {
                id: search
                objectName: "chartLibrarySearch"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                outlined: true
                leadingIcon: Qt.resolvedUrl("icons/search.svg")
                backgroundColor: root.nativeMaterialActive
                    ? (Theme.darkTheme ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(0, 0, 0, 0.045))
                    : Theme.colors.background.control
                Accessible.name: qsTrId("library.search")
                placeholderText: Accessible.name
                onTextChanged: root.resetSongPosition()
                onAccepted: root.openCurrentResult()
                Keys.onEscapePressed: search.focus = false
                Keys.onDownPressed: event => {
                    event.accepted = root.focusSong(Math.max(0, viewport.songIndex))
                }
            }

            RowLayout {
                objectName: "chartLibraryActionGroup"
                visible: root.efficientMode || library.songs.length > 0
                spacing: 2

                LibraryIconButton {
                    id: difficultyButton
                    objectName: "chartLibraryDifficultyFilter"
                    visible: library.songs.length > 0
                    active: difficultyMenu.active || root.selectedDifficulties.length > 0
                    iconSource: Qt.resolvedUrl("icons/filter.svg")
                    filledIconSource: Qt.resolvedUrl("icons/filter-fill.svg")
                    tooltip: qsTrId("action.filter")
                    onClicked: {
                        if (difficultyMenu.active)
                            difficultyMenu.close()
                        else
                            difficultyMenu.openAt(difficultyButton)
                    }

                    AppStickyPopup {
                        id: difficultyMenu
                        minimumWidth: 180

                        contentItem: Column {
                            id: difficultyOptionsColumn

                            Repeater {
                                model: root.difficultyOptions
                                delegate: AppMenuItem {
                                    objectName: "chartLibraryDifficultyOption"
                                    required property int index
                                    required property string modelData
                                    width: difficultyOptionsColumn.width
                                    focusPolicy: Qt.StrongFocus
                                    text: modelData
                                    difficultyId: index + 1
                                    checkable: true
                                    checked: root.selectedDifficulties.includes(difficultyId)
                                    onToggled: root.setDifficultySelected(difficultyId, checked)
                                }
                            }
                        }
                    }
                }

                LibraryIconButton {
                    objectName: "chartLibraryEfficientFolderButton"
                    visible: root.efficientMode
                    iconSource: Qt.resolvedUrl("icons/folder-open.svg")
                    tooltip: qsTrId("library.choose_folder")
                    onClicked: root.openDirectoryPicker()
                }

                LibraryIconButton {
                    objectName: "chartLibraryEfficientRefreshButton"
                    visible: root.efficientMode
                    iconSource: Qt.resolvedUrl("icons/refresh.svg")
                    tooltip: qsTrId("library.refresh")
                    enabled: !library.scanning && root.hasDirectory
                    onClicked: library.refresh()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Rows are created on demand: a fully instantiated library keeps
            // thousands of scene nodes alive and slows every preview frame.
            ListView {
                id: viewport
                objectName: "chartLibrarySongList"
                anchors.fill: parent
                anchors.leftMargin: root.contentMargin
                anchors.rightMargin: root.contentMargin
                clip: true
                spacing: 4
                model: root.filteredSongs
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: false
                ScrollBar.vertical: AppScrollBar {
                    parent: viewport.parent
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                }

                property int songIndex: -1

                Keys.onUpPressed: event => {
                    event.accepted = root.focusSong(songIndex - 1)
                }
                Keys.onDownPressed: event => {
                    event.accepted = root.focusSong(songIndex + 1)
                }
                Keys.onReturnPressed: root.openCurrentResult()
                Keys.onEnterPressed: root.openCurrentResult()
                Keys.onPressed: event => {
                    const contextMenuKey = event.key === Qt.Key_Menu
                        || (event.key === Qt.Key_F10 && event.modifiers === Qt.ShiftModifier)
                    const row = contextMenuKey ? itemAtIndex(songIndex) : null
                    if (row) {
                        root.showSongMenu(row, 0, row.height)
                        event.accepted = true
                    }
                }

                delegate: NavRow {
                    id: songRow
                    objectName: "chartLibrarySongRow"
                    required property int index
                    required property var modelData
                    readonly property bool currentChart: root.activeChartKey.length > 0
                        && root.activeChartKey === modelData.pathKey
                    readonly property color secondaryTextColor: chromeSelected
                        ? Theme.colors.text.primary : Theme.colors.text.secondary
                    readonly property bool hasThreeDetailLines: coverTile.visible && !variantName.visible
                        && badgeFlow.implicitHeight <= badgeFlow.badgeHeight
                    readonly property rect titleInk: hasThreeDetailLines
                        ? visibleInk(songName, titleMetrics) : Qt.rect(0, 0, 0, 0)
                    readonly property rect artistInk: hasThreeDetailLines
                        ? visibleInk(artistName, artistMetrics) : Qt.rect(0, 0, 0, 0)
                    readonly property bool alignDetailsToCover: hasThreeDetailLines
                        && titleInk.height > 0
                        && titleInk.height + artistInk.height + 2
                            <= coverTile.Layout.preferredHeight - badgeFlow.badgeHeight

                    function visibleInk(label, metrics) {
                        const displayedText = label.truncated
                            ? metrics.elidedText(label.text, Text.ElideRight, label.width) : label.text
                        return metrics.tightBoundingRect(displayedText)
                    }

                    width: viewport.width
                    height: Math.max(60, songBody.implicitHeight + 12)
                    leftPadding: 6
                    rightPadding: leftPadding
                    stateColors: root.rowStateColors
                    highlightFillsBounds: true
                    emphasizedText: true
                    text: modelData.title
                    Accessible.name: [modelData.title, artistName.text, modelData.variant,
                        currentChart ? qsTrId("library.current_editing") : ""]
                        .filter(value => value).join(", ")
                    selected: currentChart
                    highlighted: viewport.activeFocus && index === viewport.songIndex
                    onClicked: root.openSongAt(index)
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: eventPoint =>
                            root.showSongMenu(songRow, eventPoint.position.x, eventPoint.position.y)
                    }
                    Tooltip {
                        visible: songRow.hovered && (songName.truncated || artistName.truncated || variantName.truncated)
                        text: [songRow.modelData.title, artistName.text, songRow.modelData.variant]
                            .filter(value => value).join("\n")
                    }
                    contentItem: RowLayout {
                        id: songBody
                        spacing: 9
                        Rectangle {
                            id: coverTile
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            Layout.alignment: Qt.AlignVCenter
                            visible: songRow.availableWidth >= 2 * Layout.preferredWidth + songBody.spacing
                            color: Theme.colors.background.control
                            radius: Theme.compactControlRadius

                            ControlsImpl.IconImage {
                                objectName: "chartLibraryCoverFallback"
                                width: 20
                                height: 20
                                anchors.centerIn: parent
                                visible: songCover.status !== Image.Ready
                                source: Qt.resolvedUrl("icons/image.svg")
                                color: Theme.colors.text.secondary
                            }
                            Image {
                                id: songCover
                                objectName: "chartLibraryCoverThumbnail"
                                anchors.fill: parent
                                source: songRow.modelData.coverKey.length > 0
                                    ? "image://chartlibrarycover/" + (coverTile.radius / coverTile.Layout.preferredWidth)
                                        + "/" + songRow.modelData.coverKey
                                    : ""
                                sourceSize: Qt.size(96, 96)
                                asynchronous: true
                                fillMode: Image.PreserveAspectCrop
                                visible: status === Image.Ready
                            }
                        }
                        ColumnLayout {
                            id: songDetails
                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: songRow.alignDetailsToCover ? coverTile.height : -1
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 1
                            Label {
                                id: songName
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.uiFontSize + 2
                                text: songRow.modelData.title
                                textFormat: Text.PlainText
                                color: songRow.contentColor
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                                font.weight: Font.DemiBold
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                transform: Translate {
                                    y: songRow.alignDetailsToCover
                                        ? coverTile.y - songDetails.y - songName.y
                                            - songName.baselineOffset - songRow.titleInk.y
                                        : 0
                                }
                                FontMetrics {
                                    id: titleMetrics
                                    font: songName.font
                                }
                            }
                            Label {
                                id: artistName
                                Layout.fillWidth: true
                                Layout.fillHeight: songRow.alignDetailsToCover
                                Layout.preferredHeight: Theme.uiFontSize
                                text: (songRow.modelData.artist ?? "").trim()
                                textFormat: Text.PlainText
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.secondaryFontSize
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                transform: Translate {
                                    y: {
                                        if (!songRow.alignDetailsToCover || artistName.text.length === 0)
                                            return 0
                                        const titleBottom = coverTile.y - songDetails.y + songRow.titleInk.height
                                        const inkCenter = artistName.baselineOffset + songRow.artistInk.y
                                            + songRow.artistInk.height / 2
                                        return (titleBottom + badgeFlow.y) / 2 - artistName.y - inkCenter
                                    }
                                }
                                FontMetrics {
                                    id: artistMetrics
                                    font: artistName.font
                                }
                            }
                            Label {
                                id: variantName
                                objectName: "chartLibraryVariant"
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.uiFontSize
                                text: songRow.modelData.variant ?? ""
                                textFormat: Text.PlainText
                                visible: text.length > 0
                                color: songRow.secondaryTextColor
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize - 1
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            Flow {
                                id: badgeFlow
                                readonly property int badgeHeight: 16
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.minimumHeight: badgeHeight
                                spacing: 4

                                Repeater {
                                    model: songRow.modelData.difficulties.length > 0
                                        ? songRow.modelData.difficulties : [null]
                                    delegate: AbstractButton {
                                        id: badge
                                        required property var modelData
                                        readonly property bool createsDifficulty: modelData === null
                                        readonly property color badgeColor: createsDifficulty
                                            ? Theme.colors.text.secondary : Theme.difficultyColor(modelData.id)
                                        readonly property string createLabel: qsTrId("action.new")
                                        objectName: createsDifficulty
                                            ? "chartLibraryCreateDifficulty" : "chartLibraryDifficultyBadge"
                                        implicitWidth: 30
                                        implicitHeight: badgeFlow.badgeHeight
                                        padding: 0
                                        hoverEnabled: true
                                        focusPolicy: Qt.StrongFocus
                                        text: createsDifficulty ? "" : ((modelData.level ?? "").trim() || modelData.shortName)
                                        Accessible.name: createsDifficulty ? createLabel
                                            : qsTrId("action.open") + " " + qsTrId("library.difficulty." + modelData.id)
                                        Keys.enabled: createsDifficulty
                                        Keys.onReturnPressed: animateClick()
                                        Keys.onEnterPressed: animateClick()
                                        onClicked: root.openSongAt(songRow.index, modelData?.id)
                                        Tooltip {
                                            visible: badge.hovered && (badge.createsDifficulty || badgeLabel.truncated)
                                            text: badge.createsDifficulty ? badge.createLabel : badge.text
                                        }
                                        background: Rectangle {
                                            radius: Theme.smallControlRadius
                                            color: Qt.rgba(badge.badgeColor.r, badge.badgeColor.g, badge.badgeColor.b,
                                                Theme.darkTheme ? (badge.down ? 0.58 : badge.hovered ? 0.44 : 0.28)
                                                    : (badge.down ? 0.44 : badge.hovered ? 0.32 : 0.18))
                                            border.width: badge.visualFocus || Theme.darkTheme ? Theme.controlBorderWidth : 0
                                            border.color: badge.visualFocus ? Theme.colors.accent.focus
                                                : Qt.alpha(badge.badgeColor, 0.75)
                                        }
                                        contentItem: Label {
                                            id: badgeLabel
                                            text: badge.text
                                            textFormat: Text.PlainText
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            color: Theme.colors.text.primary
                                            font.family: Theme.uiFont
                                            font.pixelSize: Theme.compactFontSize
                                            font.weight: Font.Medium
                                            leftPadding: 1
                                            rightPadding: 1
                                            elide: Text.ElideRight

                                            ControlsImpl.IconImage {
                                                anchors.centerIn: parent
                                                visible: badge.createsDifficulty
                                                width: 12
                                                height: width
                                                source: badge.createsDifficulty ? Qt.resolvedUrl("icons/add.svg") : ""
                                                sourceSize: Theme.iconSourceSize(width, height)
                                                color: badgeLabel.color
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.max(0, parent.width - 2 * root.contentMargin - 16)
                visible: viewport.count === 0
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: library.scanning ? qsTrId("library.scanning")
                        : library.directoryMissing ? qsTrId("library.missing_folder")
                        : !root.hasDirectory ? qsTrId("library.no_directory")
                        : qsTrId("library.empty")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                }

                AppButton {
                    objectName: "chartLibraryEmptyFolderButton"
                    Layout.alignment: Qt.AlignHCenter
                    visible: !library.scanning && (!root.hasDirectory || library.directoryMissing)
                    text: qsTrId("library.choose_folder")
                    onClicked: root.openDirectoryPicker()
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: root.contentMargin
            Layout.rightMargin: root.contentMargin
            visible: library.songs.length > 0
            text: qsTrId("library.result_count").arg(viewport.count).arg(library.songs.length)
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize - 1
        }
    }
}
