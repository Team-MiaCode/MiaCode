pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia
import MiaCode.UI

Rectangle {
    id: root
    color: Theme.surfaceColor(Theme.colors.background.surface)

    signal difficultyRequested(url chart, int difficulty)
    readonly property alias libraryModel: library
    readonly property alias audition: player
    property var selectedSong: null
    property bool loaded: false
    readonly property real listWidth: Math.min(400, Math.max(208, width * 0.34))
    readonly property var filteredSongs: {
        const query = search.text.trim().toLocaleLowerCase()
        return query.length === 0 ? library.songs : library.songs.filter(song =>
            song.title.toLocaleLowerCase().includes(query)
            || song.artist.toLocaleLowerCase().includes(query)
            || (song.variant ?? "").toLocaleLowerCase().includes(query))
    }

    function selectSong(song) {
        if (selectedSong?.chart.toString() !== song?.chart.toString())
            player.stop()
        selectedSong = song
    }

    function selectVisibleSong() {
        if (filteredSongs.length > 0) {
            const index = Math.max(0, Math.min(list.currentIndex, filteredSongs.length - 1))
            selectSong(filteredSongs[index])
        }
    }

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
    }

    onVisibleChanged: {
        if (!visible) {
            player.stop()
        } else if (!loaded && library.directory.toString().length > 0) {
            loaded = true
            library.refresh()
        }
    }

    ChartLibraryModel {
        id: library
        onSongsChanged: {
            const path = root.selectedSong?.chart.toString()
            root.selectSong(path ? songs.find(song => song.chart.toString() === path) ?? null : null)
        }
    }

    FolderDialog {
        id: directoryPicker
        title: qsTrId("library.choose_folder")
        onAccepted: {
            root.loaded = true
            root.selectSong(null)
            library.setDirectory(selectedFolder)
        }
    }

    MediaPlayer {
        id: player
        source: root.selectedSong?.audio ?? ""
        audioOutput: AudioOutput {}
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
            AppButton {
                objectName: "chartLibraryFolderButton"
                text: qsTrId("library.choose_folder")
                onClicked: {
                    if (library.directory.toString().length > 0)
                        directoryPicker.currentFolder = library.directory
                    directoryPicker.open()
                }
            }
            AppButton {
                objectName: "chartLibraryRefreshButton"
                text: qsTrId("library.refresh")
                enabled: !library.scanning && library.directory.toString().length > 0
                onClicked: library.refresh()
            }
            Item { Layout.fillWidth: true }
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: library.directoryPath
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
            elide: Text.ElideMiddle
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ColumnLayout {
                objectName: "chartLibraryListColumn"
                Layout.preferredWidth: root.listWidth
                Layout.minimumWidth: Math.min(150, root.listWidth)
                Layout.maximumWidth: root.listWidth
                Layout.fillHeight: true
                spacing: 8

                AppTextField {
                    id: search
                    objectName: "chartLibrarySearch"
                    Layout.fillWidth: true
                    placeholderText: qsTrId("library.search")
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
                    ScrollBar.vertical: AppScrollBar {}
                    delegate: NavRow {
                        id: songRow
                        objectName: "chartLibrarySongRow"
                        required property int index
                        required property var modelData
                        width: Math.max(0, ListView.view.width - 6)
                        height: Math.max(58, implicitContentHeight + 12)
                        leftPadding: 6
                        rightPadding: 8
                        highlightFillsBounds: true
                        text: modelData.title
                        Accessible.name: [modelData.title, modelData.artist, modelData.variant]
                            .filter(value => value).join(", ")
                        selected: root.selectedSong?.chart.toString() === modelData.chart.toString()
                        highlighted: list.activeFocus && ListView.isCurrentItem
                        onClicked: {
                            list.currentIndex = index
                            root.selectSong(modelData)
                        }
                        ToolTip.visible: songRow.hovered && (songName.truncated || artistName.truncated || variantName.truncated)
                        ToolTip.text: [modelData.title, modelData.artist, modelData.variant]
                            .filter(value => value).join("\n")
                        contentItem: RowLayout {
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
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
                                    id: songName
                                    Layout.fillWidth: true
                                    text: songRow.modelData.title
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
                                    visible: text.length > 0
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize - 1
                                    elide: Text.ElideRight
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
                            : library.directory.toString().length === 0 ? qsTrId("library.choose_folder")
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
                    visible: library.songs.length > 0
                    text: qsTrId("library.result_count").arg(list.count).arg(library.songs.length)
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize - 1
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: Theme.separatorColor
            }

            Item {
                id: detailsScroll
                objectName: "chartLibraryDetailsColumn"
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 160
                Label {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    visible: root.selectedSong === null && library.songs.length > 0
                    text: qsTrId("library.select_song")
                    color: Theme.colors.text.secondary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize
                    wrapMode: Text.WordWrap
                }
                ScrollView {
                    id: songDetails
                    anchors.fill: parent
                    visible: root.selectedSong !== null
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: Math.min(songDetails.availableWidth, 760)
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: root.selectedSong?.title ?? ""
                                    color: Theme.colors.text.primary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize + 7
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.selectedSong?.artist ?? ""
                                    visible: text.length > 0
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    visible: (root.selectedSong?.bpm ?? 0) > 0
                                    text: "BPM " + (root.selectedSong?.bpm ?? "")
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize
                                }
                            }

                            Image {
                                source: root.selectedSong?.cover ?? ""
                                visible: status === Image.Ready || status === Image.Loading
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                sourceSize: Qt.size(384, 384)
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredWidth: Math.min(168, songDetails.availableWidth * 0.34)
                                Layout.preferredHeight: Layout.preferredWidth
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: player.source.toString().length > 0
                            AppButton {
                                objectName: "chartLibraryAuditionButton"
                                enabled: player.duration > 0 && player.mediaStatus !== MediaPlayer.NoMedia
                                    && player.mediaStatus !== MediaPlayer.LoadingMedia
                                    && player.mediaStatus !== MediaPlayer.InvalidMedia
                                text: player.playbackState === MediaPlayer.PlayingState
                                    ? qsTrId("library.pause") : qsTrId("library.audition")
                                onClicked: {
                                    if (player.playbackState === MediaPlayer.PlayingState)
                                        player.pause()
                                    else
                                        player.play()
                                }
                            }
                            AppSlider {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 32
                                from: 0
                                to: Math.max(1, player.duration)
                                enabled: player.seekable
                                value: player.position
                                onMoved: player.position = value
                            }
                            Label {
                                text: root.formatTime(player.position) + " / " + root.formatTime(player.duration)
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: player.source.toString().length > 0 && player.error !== MediaPlayer.NoError
                            text: player.errorString
                            color: Theme.colors.text.secondary
                            wrapMode: Text.Wrap
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: root.selectedSong?.difficulties ?? []
                                AppButton {
                                    id: difficultyButton
                                    required property var modelData
                                    objectName: "libraryDifficulty" + modelData.id
                                    text: qsTrId("library.difficulty." + modelData.id)
                                        + (modelData.level ? " " + modelData.level : "")
                                    contentItem: RowLayout {
                                        spacing: 5
                                        DifficultySwatch { difficultyId: difficultyButton.modelData.id }
                                        Text {
                                            text: difficultyButton.text
                                            font: difficultyButton.font
                                            color: Theme.colors.text.primary
                                        }
                                    }
                                    onClicked: {
                                        player.stop()
                                        root.difficultyRequested(root.selectedSong.chart, modelData.id)
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
