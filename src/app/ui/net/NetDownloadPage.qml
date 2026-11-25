pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    required property var net
    readonly property bool batchSelecting: net.selectedCount > 0
    readonly property string activityText: net.downloading ? net.progressText
        : net.busy ? qsTrId("net.ui.searching") : net.probing ? net.connectionText
        : net.previewing ? qsTrId("net.online_preview_loading") : net.statusText
    readonly property bool hasQuery: searchField.text.trim().length > 0 || uploader.text.trim().length > 0
        || tag.text.trim().length > 0 || song.text.trim().length > 0

    function searchParameters() {
        return {search: searchField.text, uploader: uploader.text, tag: tag.text, title: song.text,
            caseSensitive: !fuzzy.checked}
    }

    function runQuery() {
        if (net.working)
            return
        if (!hasQuery) {
            searchField.dismissSuggestions()
            return
        }
        searchField.suppressSuggestionsAfterSearch()
        filterPopup.close()
        if (batchSelecting) net.selectAll(false)
        const parameters = searchParameters()
        parameters.forceRefresh = refresh.checked
        parameters.sort = sorting.currentValue
        net.query(parameters)
    }

    onVisibleChanged: {
        if (!visible) {
            filterPopup.close()
            searchField.dismissSuggestions()
        }
    }

    Keys.onEscapePressed: event => {
        event.accepted = root.batchSelecting && !root.net.working
        if (event.accepted) root.net.selectAll(false)
    }

    AppToast {
        id: completionToast
        parent: root.Overlay.overlay
        holdDuration: Theme.progressToastHoldDuration
    }

    Connections {
        target: root.net
        function onDownloadFinished(message, failed) {
            if (root.visible) completionToast.present(failed ? "error" : "information", message)
        }
    }

    component Caption: Text {
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        elide: Text.ElideRight
    }

    component FilterField: NetFormField {
        id: field
        property alias text: fieldInput.text
        property alias input: fieldInput
        AppTextField {
            id: fieldInput
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            outlined: true
            Accessible.name: field.label
            onAccepted: root.runQuery()
        }
    }

    Shortcut {
        sequence: "/"
        enabled: root.visible && !root.net.working && !filterPopup.active
            && !searchField.activeFocus && !outputDirectory.activeFocus
        onActivated: {
            searchField.focusForSearch()
            searchField.selectAll()
        }
    }

    AppStickyPopup {
        id: filterPopup
        parent: filterButton
        width: Math.min(336, Math.max(0, root.width - 2 * Theme.dialogPadding))
        padding: 12
        margins: Theme.menuPadding
        onOpened: uploader.input.forceActiveFocus()

        contentItem: ColumnLayout {
            spacing: 8
            enabled: !root.net.working

            FilterField { id: uploader; label: qsTrId("net.ui.user_id") }
            FilterField { id: tag; label: qsTrId("net.ui.tag") }
            FilterField { id: song; label: qsTrId("net.ui.song_title") }

            NetFormField {
                label: qsTrId("net.ui.sort")
                AppComboBox {
                    id: sorting
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    outlined: true
                    Accessible.name: qsTrId("net.ui.sort")
                    textRole: "label"
                    valueRole: "value"
                    onActivated: root.net.setSort(currentValue)
                    model: [
                        {label: qsTrId("net.ui.sort_title_asc"), value: "title_asc"},
                        {label: qsTrId("net.ui.sort_title_desc"), value: "title_desc"},
                        {label: qsTrId("net.ui.sort_level_desc"), value: "level_desc"},
                        {label: qsTrId("net.ui.sort_level_asc"), value: "level_asc"},
                        {label: qsTrId("net.sort_status_ascending"), value: "status_asc"},
                        {label: qsTrId("net.sort_status_descending"), value: "status_desc"}
                    ]
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppCheckBox { id: fuzzy; checked: true; text: qsTrId("net.ui.fuzzy") }
                AppCheckBox { id: refresh; text: qsTrId("net.ui.force_refresh") }
            }

            Caption {
                Layout.fillWidth: true
                text: root.net.connectionText || qsTrId("net.ui.untested")
            }

            RowLayout {
                Layout.fillWidth: true
                NetPageButton {
                    text: qsTrId("net.ui.probe")
                    onClicked: root.net.probe()
                }
                Item { Layout.fillWidth: true }
                NetPageButton {
                    text: qsTrId("net.ui.query")
                    emphasized: true
                    enabled: root.hasQuery
                    onClicked: root.runQuery()
                }
            }
        }
    }

    Flickable {
        id: page
        anchors.fill: parent
        contentWidth: width
        contentHeight: body.height + 2 * Theme.dialogPadding
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar { id: pageScrollBar }

        ColumnLayout {
            id: body
            x: Theme.dialogPadding
            y: Theme.dialogPadding
            width: Math.max(0, page.width - 2 * Theme.dialogPadding)
            height: Math.max(page.height - 2 * Theme.dialogPadding, implicitHeight)
            spacing: 16

            PanelHeader {
                Layout.fillWidth: true
                Layout.leftMargin: -10
                Layout.rightMargin: -10
                title: qsTrId("net.ui.download_page")
                sidebarTitle: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                enabled: !root.net.working

                NetSearchField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    suggestions: root.net.suggestions
                    onSuggestionsRequested: root.net.requestSuggestions(root.searchParameters())
                    onSuggestionsDismissed: root.net.clearSuggestions()
                    onSearchRequested: root.runQuery()
                }

                NetPageButton {
                    id: filterButton
                    text: qsTrId("net.ui.query_options")
                    iconSource: Qt.resolvedUrl("icons/chevron-down.svg")
                    selected: filterPopup.active
                    onClicked: {
                        if (filterPopup.active) {
                            filterPopup.close()
                        } else {
                            searchField.dismissSuggestions()
                            filterPopup.open()
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                Caption {
                    Layout.fillWidth: true
                    text: qsTrId("net.ui.results").arg(root.net.resultCount)
                }

                NetResultTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 150
                    model: root.net
                    interactive: !root.net.working
                    selecting: root.batchSelecting
                    canSelectAll: root.net.downloadableCount > 0
                    selectionState: root.net.selectedCount === 0 ? Qt.Unchecked
                        : root.net.selectedCount === root.net.downloadableCount ? Qt.Checked : Qt.PartiallyChecked
                    columns: [
                        {title: "#", width: 24, number: true},
                        {title: qsTrId("net.select"), width: 44, checkbox: true},
                        {title: qsTrId("net.title"), role: "title", weight: 30, minimum: 120},
                        {title: qsTrId("net.ui.artist"), role: "artist", weight: 20, minimum: 72, wrap: true},
                        {title: qsTrId("net.designer"), role: "designer", weight: 20, minimum: 72, wrap: true},
                        {title: qsTrId("qml.level"), role: "levels", width: 72, alignment: Text.AlignHCenter},
                        {title: qsTrId("net.ui.download_one"), width: 48, download: true},
                        {title: qsTrId("net.ui.preview_column"), width: 48, preview: true}
                    ]
                    onSelectionChanged: (chartId, selected) => root.net.setSelected(chartId, selected)
                    onSelectAllRequested: selected => root.net.selectAll(selected)
                    onPreviewRequested: chartId => root.net.preview(chartId, pv.checked)
                    onDownloadRequested: chartId => root.net.activateDownload(chartId, pv.checked, zip.checked)

                    Caption {
                        anchors.centerIn: parent
                        width: Math.max(0, parent.width - 48)
                        visible: root.net.resultCount === 0
                        text: qsTrId("net.ui.query_hint")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.separatorColor
                }
                NetFormField {
                    label: qsTrId("net.output_directory")
                    enabled: !root.net.working
                    AppTextField {
                        id: outputDirectory
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.net.outputDirectory
                        outlined: true
                        Accessible.name: qsTrId("net.output_directory")
                        onEditingFinished: root.net.outputDirectory = text
                    }
                    NetPageButton {
                        text: qsTrId("net.ui.browse")
                        onClicked: root.net.browseOutputDirectory()
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 16
                    enabled: !root.net.working
                    AppCheckBox { id: pv; checked: true; text: qsTrId("net.download_pv") }
                    AppCheckBox { id: zip; text: qsTrId("net.ui.create_zip") }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 4
                        Caption {
                            Layout.fillWidth: true
                            text: root.net.errorText
                                ? (root.net.downloading ? root.activityText + " · " : "") + root.net.errorText
                                : root.activityText
                            color: root.net.errorText.length > 0 ? Theme.colors.danger.primary : Theme.colors.text.secondary
                            visible: text.length > 0
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: root.net.downloading
                            ProgressBar {
                                id: downloadProgress
                                Layout.fillWidth: true
                                Layout.preferredWidth: Layout.maximumWidth
                                Layout.maximumWidth: Theme.progressToastWidth * 0.65
                                Layout.preferredHeight: Theme.progressToastBarHeight
                                padding: 0
                                value: Math.max(0, root.net.progress)
                                indeterminate: !root.net.paused && root.net.progress < 0
                                Accessible.name: qsTrId("net.ui.file_progress")

                                background: Rectangle {
                                    radius: height / 2
                                    color: Theme.colors.border.control
                                }
                                contentItem: Item {
                                    Rectangle {
                                        id: progressFill
                                        property real sweep: 0
                                        width: parent.width * (downloadProgress.indeterminate ? 0.25 : downloadProgress.position)
                                        height: parent.height
                                        x: downloadProgress.indeterminate ? (parent.width - width) * sweep
                                            : downloadProgress.mirrored ? parent.width - width : 0
                                        radius: height / 2
                                        color: root.net.paused ? Theme.colors.text.secondary : Theme.colors.accent.primary

                                        SequentialAnimation on sweep {
                                            running: downloadProgress.visible && downloadProgress.indeterminate
                                            loops: Animation.Infinite
                                            NumberAnimation { from: 0; to: 1; duration: 850; easing.type: Easing.InOutSine }
                                            NumberAnimation { from: 1; to: 0; duration: 850; easing.type: Easing.InOutSine }
                                        }
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                    NetPageButton {
                        visible: root.net.paused
                        text: qsTrId("net.ui.resume")
                        onClicked: root.net.resumeDownload()
                    }
                    NetPageButton {
                        visible: root.net.working
                        text: qsTrId("net.ui.cancel_task")
                        onClicked: root.net.cancelTask()
                    }
                    NetPageButton {
                        visible: root.batchSelecting && !root.net.working
                        text: qsTrId("action.cancel")
                        onClicked: root.net.selectAll(false)
                    }
                    NetPageButton {
                        visible: !root.net.working
                        iconSource: Qt.resolvedUrl("icons/download.svg")
                        text: root.batchSelecting ? qsTrId("net.ui.download_selection_count").arg(root.net.selectedCount)
                            : qsTrId("net.ui.download_one")
                        emphasized: true
                        enabled: root.batchSelecting
                        onClicked: root.net.downloadSelected(pv.checked, zip.checked)
                    }
                }
            }
        }
    }
}
