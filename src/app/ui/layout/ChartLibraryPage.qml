pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Dialogs
import QtQuick.Layouts
import MiaCode.UI

Rectangle {
    id: root
    color: Theme.surfaceColor(Theme.colors.background.surface)

    signal difficultyRequested(url chart, int difficulty)
    required property string activeChartPath
    readonly property alias libraryModel: library
    readonly property string activeChartKey: library.chartPathKey(activeChartPath)
    readonly property real scrollBarGutter: libraryScrollBar.width
    property var selectedSong: null
    property bool loaded: false
    property int selectedDifficulty: 0
    readonly property var difficultyOptions: [qsTrId("library.all_difficulties"),
        qsTrId("library.difficulty.1"), qsTrId("library.difficulty.2"),
        qsTrId("library.difficulty.3"), qsTrId("library.difficulty.4"),
        qsTrId("library.difficulty.5"), qsTrId("library.difficulty.6"),
        qsTrId("library.difficulty.7")]
    readonly property var filteredSongs: {
        const query = search.text.trim().toLocaleLowerCase()
        const difficulty = root.selectedDifficulty
        return library.songs.filter(song =>
            (difficulty <= 0 || song.difficulties.some(entry => entry.id === difficulty))
            && (query.length === 0 || [song.title, song.artist, song.variant, song.songId]
                .some(value => value?.toLocaleLowerCase().includes(query))))
    }

    onSelectedDifficultyChanged: {
        list.currentIndex = -1
        list.positionViewAtBeginning()
    }

    function selectVisibleSong() {
        if (filteredSongs.length > 0) {
            const index = Math.max(0, Math.min(list.currentIndex, filteredSongs.length - 1))
            selectedSong = filteredSongs[index]
        }
    }

    onVisibleChanged: {
        if (visible && !loaded && library.directory.toString().length > 0) {
            loaded = true
            library.refresh()
        }
    }

    ChartLibraryModel {
        id: library
        onSongsChanged: {
            const path = root.selectedSong?.chart.toString()
            root.selectedSong = path ? songs.find(song => song.chart.toString() === path) ?? null : null
        }
    }

    FolderDialog {
        id: directoryPicker
        title: qsTrId("library.choose_folder")
        onAccepted: {
            root.loaded = true
            root.selectedSong = null
            root.selectedDifficulty = 0
            library.setDirectory(selectedFolder)
        }
    }

    PanelHeader {
        id: heading
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
        anchors.leftMargin: 10
        anchors.rightMargin: 14
        anchors.bottomMargin: 10
        spacing: 9

        RowLayout {
            Layout.fillWidth: true
            Layout.rightMargin: root.scrollBarGutter
            spacing: 8
            AppButton {
                objectName: "chartLibraryFolderButton"
                Layout.fillWidth: true
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
                iconSource: Qt.resolvedUrl("icons/refresh.svg")
                tooltip: qsTrId("library.refresh")
                enabled: !library.scanning && library.directory.toString().length > 0
                onClicked: library.refresh()
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.rightMargin: root.scrollBarGutter
            visible: text.length > 0
            text: library.directoryPath
            textFormat: Text.PlainText
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            elide: Text.ElideMiddle
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
                Layout.rightMargin: root.scrollBarGutter
                spacing: 8

                AppTextField {
                    id: search
                    objectName: "chartLibrarySearch"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    outlined: true
                    Accessible.name: qsTrId("library.search")
                    placeholderText: activeFocus ? "" : Accessible.name
                    onTextChanged: {
                        list.currentIndex = -1
                        list.positionViewAtBeginning()
                    }
                    onAccepted: root.selectVisibleSong()
                    Keys.onDownPressed: {
                        if (list.count > 0) {
                            list.currentIndex = Math.max(0, list.currentIndex)
                            list.forceActiveFocus()
                        }
                    }
                }

                IconButton {
                    objectName: "chartLibraryDifficultyFilter"
                    Layout.preferredWidth: Theme.controlMinHeight
                    Layout.preferredHeight: Theme.controlMinHeight
                    visible: library.songs.length > 0
                    active: difficultyMenu.active || root.selectedDifficulty > 0
                    iconSource: Qt.resolvedUrl("icons/sliders-horizontal.svg")
                    filledIconSource: Qt.resolvedUrl("icons/sliders-horizontal-fill.svg")
                    tooltip: qsTrId("dialog.batch_export.difficulty") + " · "
                        + root.difficultyOptions[root.selectedDifficulty]
                    onClicked: difficultyMenu.open()

                    AppMenu {
                        id: difficultyMenu
                        x: parent.width - width
                        y: parent.height

                        ButtonGroup {
                            id: difficultyGroup
                        }

                        Repeater {
                            model: root.difficultyOptions
                            delegate: AppMenuItem {
                                objectName: "chartLibraryDifficultyOption"
                                required property int index
                                required property string modelData
                                text: modelData
                                difficultyId: index
                                checkable: true
                                ButtonGroup.group: difficultyGroup
                                checked: root.selectedDifficulty === index
                                onTriggered: root.selectedDifficulty = index
                            }
                        }
                    }
                }
            }

            ListView {
                id: list
                objectName: "chartLibrarySongList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.filteredSongs
                spacing: 4
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                Keys.onReturnPressed: root.selectVisibleSong()
                Keys.onEnterPressed: root.selectVisibleSong()
                ScrollBar.vertical: AppScrollBar { id: libraryScrollBar }
                delegate: NavRow {
                    id: songRow
                    objectName: "chartLibrarySongRow"
                    required property int index
                    required property var modelData
                    readonly property bool currentChart: root.activeChartKey.length > 0
                        && root.activeChartKey === modelData.pathKey
                    width: Math.max(0, ListView.view.width - root.scrollBarGutter)
                    height: Math.max(60, songBody.implicitHeight + 12)
                    leftPadding: 6
                    rightPadding: 8
                    highlightFillsBounds: true
                    text: modelData.title
                    Accessible.name: [modelData.title, modelData.artist, modelData.variant, modelData.songId,
                        currentChart ? qsTrId("library.current_editing") : ""]
                        .filter(value => value).join(", ")
                    selected: root.selectedSong?.chart.toString() === modelData.chart.toString()
                    highlighted: list.activeFocus && ListView.isCurrentItem
                    onClicked: {
                        list.currentIndex = index
                        root.selectedSong = modelData
                    }
                    ToolTip.visible: songRow.hovered && (songName.truncated || artistName.truncated || variantName.truncated)
                    ToolTip.text: [modelData.title, modelData.artist, modelData.variant]
                        .filter(value => value).join("\n")
                    Rectangle {
                        objectName: "chartLibraryCurrentSongMarker"
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 9
                        anchors.bottomMargin: 9
                        width: 2
                        radius: 1
                        color: Theme.colors.accent.primary
                        visible: songRow.currentChart
                    }
                    contentItem: RowLayout {
                        id: songBody
                        spacing: 9
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            color: Theme.colors.background.control
                            radius: Theme.compactControlRadius
                            clip: true

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
                                source: songRow.modelData.cover
                                sourceSize: Qt.size(96, 96)
                                asynchronous: true
                                fillMode: Image.PreserveAspectCrop
                                visible: status === Image.Ready
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: [songRow.modelData.songId, songRow.modelData.kind]
                                    .filter(Boolean).join("  ·  ")
                                visible: text.length > 0
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.secondaryFontSize
                            }
                            Label {
                                id: songName
                                Layout.fillWidth: true
                                text: songRow.modelData.title
                                textFormat: Text.PlainText
                                color: Theme.colors.text.primary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize + 1
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Label {
                                id: artistName
                                Layout.fillWidth: true
                                text: songRow.modelData.artist
                                textFormat: Text.PlainText
                                visible: text.length > 0
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize - 1
                                elide: Text.ElideRight
                            }
                            Label {
                                id: variantName
                                objectName: "chartLibraryVariant"
                                Layout.fillWidth: true
                                text: songRow.modelData.variant ?? ""
                                textFormat: Text.PlainText
                                visible: text.length > 0
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize - 1
                                elide: Text.ElideRight
                            }
                            Flow {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                visible: levelBadges.count > 0
                                spacing: 5
                                Repeater {
                                    id: levelBadges
                                    model: songRow.modelData.difficulties
                                    delegate: AbstractButton {
                                        id: badge
                                        required property var modelData
                                        readonly property color badgeColor: Theme.difficultyColor(modelData.id)
                                        readonly property string difficultyName: qsTrId("library.difficulty." + modelData.id)
                                        objectName: "chartLibraryDifficultyBadge"
                                        implicitWidth: Math.max(26, implicitContentWidth + 10)
                                        implicitHeight: implicitContentHeight + 2
                                        padding: 0
                                        hoverEnabled: true
                                        focusPolicy: Qt.StrongFocus
                                        text: (modelData.level ?? "").trim() || modelData.shortName
                                        Accessible.name: qsTrId("action.open") + " " + difficultyName
                                        ToolTip.visible: hovered
                                        ToolTip.text: Accessible.name
                                        onClicked: {
                                            list.currentIndex = songRow.index
                                            root.selectedSong = songRow.modelData
                                            root.difficultyRequested(songRow.modelData.chart, modelData.id)
                                        }
                                        background: Rectangle {
                                            radius: Theme.smallControlRadius
                                            color: Qt.rgba(badge.badgeColor.r, badge.badgeColor.g, badge.badgeColor.b,
                                                Theme.darkTheme ? (badge.hovered || badge.down ? 0.58 : 0.40)
                                                    : (badge.hovered || badge.down ? 0.42 : 0.25))
                                            border.width: badge.visualFocus || Theme.darkTheme ? Theme.controlBorderWidth : 0
                                            border.color: badge.visualFocus ? Theme.colors.accent.focus
                                                : Qt.lighter(badge.badgeColor, 1.18)
                                        }
                                        contentItem: Label {
                                            text: badge.text
                                            textFormat: Text.PlainText
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            color: Theme.colors.text.primary
                                            font.family: Theme.uiFont
                                            font.pixelSize: Theme.secondaryFontSize
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
                    visible: library.scanning || list.count === 0
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
                Layout.rightMargin: root.scrollBarGutter
                visible: library.songs.length > 0
                text: qsTrId("library.result_count").arg(list.count).arg(library.songs.length)
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize - 1
            }
        }
    }
}
