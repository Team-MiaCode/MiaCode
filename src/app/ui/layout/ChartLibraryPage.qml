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
    readonly property alias libraryModel: library
    readonly property string activeChartKey: library.chartPathKey(activeChartPath)
    readonly property real contentMargin: 10
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
        viewport.currentIndex = -1
        viewport.contentItem.contentY = 0
    }

    function focusSong(index) {
        const row = songs.itemAt(index)
        if (!row)
            return false

        viewport.currentIndex = index
        viewport.forceActiveFocus()
        const scroll = viewport.contentItem
        if (row.y < scroll.contentY)
            scroll.contentY = row.y
        else if (row.y + row.height > scroll.contentY + scroll.height)
            scroll.contentY = Math.min(row.y, row.y + row.height - scroll.height)
        return true
    }

    function openSongAt(index, preferredDifficultyId) {
        const song = filteredSongs[index]
        if (!song)
            return
        viewport.currentIndex = index
        const matchingDifficulty = song.difficulties.find(entry => selectedDifficulties.includes(entry.id))
        root.chartRequested(song.chart, preferredDifficultyId ?? matchingDifficulty?.id ?? 0)
    }

    function openCurrentResult() {
        const index = Math.max(0, Math.min(viewport.currentIndex, filteredSongs.length - 1))
        openSongAt(index)
    }

    function showSongMenu(row, x, y) {
        const point = row.mapToItem(root, x, y)
        songMenu.chart = row.modelData.chart
        songMenu.popup(root, point.x, point.y)
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

    PanelHeader {
        id: heading
        objectName: "chartLibraryHeader"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: implicitHeight
        title: qsTrId("library.title")
        sidebarTitle: true
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        spacing: 9

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: root.contentMargin
            Layout.rightMargin: root.contentMargin
            spacing: 8
            AppButton {
                objectName: "chartLibraryFolderButton"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                backgroundFillsWidth: true
                text: qsTrId("library.choose_folder")
                onClicked: {
                    const folder = library.pickerDirectory(root.activeChartPath)
                    if (folder.toString().length > 0)
                        directoryPicker.currentFolder = folder
                    directoryPicker.open()
                }
            }
            IconButton {
                objectName: "chartLibraryRefreshButton"
                Layout.preferredWidth: Theme.controlMinHeight
                Layout.preferredHeight: Theme.controlMinHeight
                backgroundFillsWidth: true
                iconSource: Qt.resolvedUrl("icons/refresh.svg")
                tooltip: qsTrId("library.refresh")
                enabled: !library.scanning && library.directory.toString().length > 0
                onClicked: library.refresh()
            }
        }

        ColumnLayout {
            objectName: "chartLibraryListColumn"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            spacing: 8

            RowLayout {
                objectName: "chartLibrarySearchRow"
                Layout.fillWidth: true
                Layout.leftMargin: root.contentMargin
                Layout.rightMargin: root.contentMargin
                spacing: 8

                AppTextField {
                    id: search
                    objectName: "chartLibrarySearch"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    outlined: true
                    Accessible.name: qsTrId("library.search")
                    placeholderText: activeFocus ? "" : Accessible.name
                    onTextChanged: root.resetSongPosition()
                    onAccepted: root.openCurrentResult()
                    Keys.onDownPressed: event => {
                        event.accepted = root.focusSong(Math.max(0, viewport.currentIndex))
                    }
                }

                IconButton {
                    id: difficultyButton
                    objectName: "chartLibraryDifficultyFilter"
                    Layout.preferredWidth: Theme.controlMinHeight
                    Layout.preferredHeight: Theme.controlMinHeight
                    backgroundFillsWidth: true
                    visible: library.songs.length > 0
                    active: difficultyMenu.active || root.selectedDifficulties.length > 0
                    iconSource: Qt.resolvedUrl("icons/sliders-horizontal.svg")
                    filledIconSource: Qt.resolvedUrl("icons/sliders-horizontal-fill.svg")
                    tooltip: qsTrId("dialog.batch_export.difficulty") + " · "
                        + (root.selectedDifficulties.length === 0 ? qsTrId("library.all_difficulties")
                            : root.difficultyOptions.filter((_, index) =>
                                root.selectedDifficulties.includes(index + 1)).join(", "))
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
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ScrollView {
                    id: viewport
                    objectName: "chartLibrarySongList"
                    anchors.fill: parent
                    contentWidth: availableWidth
                    leftPadding: root.contentMargin
                    rightPadding: root.contentMargin
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical: AppScrollBar {
                        parent: viewport
                        anchors.top: viewport.top
                        anchors.bottom: viewport.bottom
                        anchors.right: viewport.right
                    }

                    property int currentIndex: -1

                    Keys.onUpPressed: event => {
                        event.accepted = root.focusSong(currentIndex - 1)
                    }
                    Keys.onDownPressed: event => {
                        event.accepted = root.focusSong(currentIndex + 1)
                    }
                    Keys.onReturnPressed: root.openCurrentResult()
                    Keys.onEnterPressed: root.openCurrentResult()
                    Keys.onPressed: event => {
                        const contextMenuKey = event.key === Qt.Key_Menu
                            || (event.key === Qt.Key_F10 && event.modifiers === Qt.ShiftModifier)
                        const row = contextMenuKey ? songs.itemAt(currentIndex) : null
                        if (row) {
                            root.showSongMenu(row, 0, row.height)
                            event.accepted = true
                        }
                    }

                    Column {
                        id: songColumn
                        width: viewport.availableWidth
                        spacing: 4

                        Repeater {
                            id: songs
                            model: root.filteredSongs

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

                                width: songColumn.width
                                height: Math.max(60, songBody.implicitHeight + 12)
                                leftPadding: 6
                                rightPadding: leftPadding
                                highlightFillsBounds: true
                                emphasizedText: true
                                text: modelData.title
                                Accessible.name: [modelData.title, artistName.text, modelData.variant,
                                    currentChart ? qsTrId("library.current_editing") : ""]
                                    .filter(value => value).join(", ")
                                selected: currentChart
                                highlighted: viewport.activeFocus && index === viewport.currentIndex
                                onClicked: root.openSongAt(index)
                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: eventPoint =>
                                        root.showSongMenu(songRow, eventPoint.position.x, eventPoint.position.y)
                                }
                                ToolTip.visible: songRow.hovered && (songName.truncated || artistName.truncated || variantName.truncated)
                                ToolTip.text: [modelData.title, artistName.text, modelData.variant]
                                    .filter(value => value).join("\n")
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
                                                    ToolTip.visible: badge.hovered && (badge.createsDifficulty || badgeLabel.truncated)
                                                    ToolTip.text: badge.createsDifficulty ? badge.createLabel : badge.text
                                                    background: Rectangle {
                                                        radius: Theme.smallControlRadius
                                                        color: Qt.rgba(badge.badgeColor.r, badge.badgeColor.g, badge.badgeColor.b,
                                                            Theme.darkTheme ? (badge.hovered || badge.down ? 0.48 : 0.28)
                                                                : (badge.hovered || badge.down ? 0.35 : 0.18))
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
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - 16)
                    visible: library.scanning || songs.count === 0
                    text: library.scanning ? qsTrId("library.scanning")
                        : library.directoryMissing ? qsTrId("library.missing_folder")
                        : library.directory.toString().length === 0 ? qsTrId("library.no_directory")
                        : qsTrId("library.empty")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                }
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: root.contentMargin
                Layout.rightMargin: root.contentMargin
                visible: library.songs.length > 0
                text: qsTrId("library.result_count").arg(songs.count).arg(library.songs.length)
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize - 1
            }
        }
    }
}
