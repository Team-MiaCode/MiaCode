pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    required property var net
    property bool logVisible: false
    readonly property bool transferVisible: net.working || net.paused || net.progress > 0

    function runQuery() {
        startDate.commitDate()
        endDate.commitDate()
        net.query({uploader: uploader.text, tag: tag.text, title: song.text,
            startDate: startDate.text, endDate: endDate.text,
            caseSensitive: !fuzzy.checked, forceRefresh: refresh.checked,
            sort: sorting.currentValue})
    }

    component Caption: Text {
        color: Theme.colors.text.secondary
        font.family: Theme.uiFont
        font.pixelSize: Theme.secondaryFontSize
        elide: Text.ElideRight
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                enabled: !root.net.working

                GridLayout {
                    id: searchFields
                    Layout.fillWidth: true
                    columns: width >= 520 ? 3 : 2
                    uniformCellWidths: true
                    columnSpacing: 12
                    rowSpacing: 10

                    NetFormField {
                        label: qsTrId("net.ui.user_id")
                        AppTextField {
                            id: uploader
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            outlined: true
                            Accessible.name: qsTrId("net.ui.user_id")
                            onAccepted: root.runQuery()
                        }
                    }
                    NetFormField {
                        label: qsTrId("net.ui.tag")
                        AppTextField {
                            id: tag
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            outlined: true
                            Accessible.name: qsTrId("net.ui.tag")
                            onAccepted: root.runQuery()
                        }
                    }
                    NetFormField {
                        Layout.columnSpan: searchFields.columns === 2 ? 2 : 1
                        label: qsTrId("net.ui.song_title")
                        AppTextField {
                            id: song
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            outlined: true
                            Accessible.name: qsTrId("net.ui.song_title")
                            onAccepted: root.runQuery()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: advancedFields.implicitHeight + 24
                    visible: filterToggle.checked
                    radius: Theme.controlRadius
                    color: Theme.surfaceColor(Theme.colors.background.panel)

                    ColumnLayout {
                        id: advancedFields
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            NetFormField {
                                label: qsTrId("net.start")
                                AppDateField {
                                    id: startDate
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    outlined: true
                                    Accessible.name: qsTrId("net.ui.start_date")
                                }
                            }
                            NetFormField {
                                label: qsTrId("net.ui.end")
                                AppDateField {
                                    id: endDate
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    outlined: true
                                    Accessible.name: qsTrId("net.ui.end_date")
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Caption { text: qsTrId("net.ui.sort") }
                            AppComboBox {
                                id: sorting
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                outlined: true
                                textRole: "label"
                                valueRole: "value"
                                onActivated: root.net.setSort(currentValue)
                                model: [
                                    {label: qsTrId("net.ui.sort_uploaded_desc"), value: "uploaded_desc"},
                                    {label: qsTrId("net.ui.sort_uploaded_asc"), value: "uploaded_asc"},
                                    {label: qsTrId("net.ui.sort_level_desc"), value: "level_desc"},
                                    {label: qsTrId("net.ui.sort_level_asc"), value: "level_asc"},
                                    {label: qsTrId("net.ui.sort_title_asc"), value: "title_asc"},
                                    {label: qsTrId("net.ui.sort_title_desc"), value: "title_desc"},
                                    {label: qsTrId("net.sort_status_ascending"), value: "status_asc"},
                                    {label: qsTrId("net.sort_status_descending"), value: "status_desc"}
                                ]
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 12
                            AppCheckBox { id: fuzzy; checked: true; text: qsTrId("net.ui.fuzzy") }
                            AppCheckBox { id: refresh; text: qsTrId("net.ui.force_refresh") }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            NetPageButton {
                                checkable: false
                                text: qsTrId("net.ui.probe")
                                onClicked: root.net.probe()
                            }
                            Caption {
                                Layout.fillWidth: true
                                text: root.net.connectionText || qsTrId("net.ui.untested")
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    NetPageButton {
                        id: filterToggle
                        checkable: true
                        text: qsTrId("net.ui.query_options")
                    }
                    Item { Layout.fillWidth: true }
                    NetPageButton {
                        text: qsTrId("net.ui.query")
                        onClicked: root.runQuery()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Caption {
                        Layout.fillWidth: true
                        text: qsTrId("net.ui.result_count").arg(root.net.resultCount).arg(root.net.selectedCount)
                    }
                    NetPageButton {
                        checkable: false
                        text: qsTrId("net.select_all")
                        enabled: !root.net.working && root.net.resultCount > 0
                        onClicked: root.net.selectAll(true)
                    }
                    NetPageButton {
                        checkable: false
                        text: qsTrId("net.ui.clear_selection")
                        enabled: !root.net.working && root.net.resultCount > 0
                        onClicked: root.net.selectAll(false)
                    }
                }

                NetResultTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 150
                    model: root.net
                    interactive: !root.net.working
                    columns: [
                        {title: "#", width: 24},
                        {title: qsTrId("net.select"), width: 44, checkbox: true},
                        {title: qsTrId("net.title"), role: "title", weight: 30, minimum: 120},
                        {title: qsTrId("net.ui.artist"), role: "artist", weight: 20, minimum: 72, wrap: true},
                        {title: qsTrId("net.designer"), role: "designer", weight: 20, minimum: 72, wrap: true},
                        {title: qsTrId("qml.level"), role: "levels", width: 72, alignment: Text.AlignHCenter},
                        {title: qsTrId("net.ui.uploaded_at"), role: "uploadedAt", width: 110, alignment: Text.AlignHCenter},
                        {title: qsTrId("net.ui.state"), role: "itemState", width: 68, alignment: Text.AlignHCenter},
                        {title: qsTrId("net.online_preview"), width: 68, preview: true}
                    ]
                    onSelectionChanged: (chartId, selected) => root.net.setSelected(chartId, selected)
                    onPreviewRequested: chartId => root.net.preview(chartId, pv.checked)

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

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                visible: root.logVisible
                clip: true
                TextArea {
                    text: root.net.logText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: Text.Wrap
                    color: Theme.colors.text.primary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.secondaryFontSize
                    background: Rectangle { color: Theme.surfaceColor(Theme.colors.background.control) }
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
                    Caption {
                        Layout.fillWidth: true
                        text: root.net.errorText || root.net.statusText
                            || (root.transferVisible ? root.net.progressText || Math.round(root.net.progress * 100) + "%" : "")
                        color: root.net.errorText.length > 0 ? Theme.colors.danger.primary : Theme.colors.text.secondary
                    }
                    NetPageButton {
                        text: qsTrId("net.ui.view_log")
                        checkable: true
                        checked: root.logVisible
                        onClicked: root.logVisible = checked
                    }
                    NetPageButton {
                        visible: root.net.paused
                        text: qsTrId("net.ui.resume")
                        onClicked: root.net.resumeDownload()
                    }
                    NetPageButton {
                        text: root.net.working ? qsTrId("net.ui.cancel_task") : qsTrId("net.download_selected")
                        enabled: root.net.working || root.net.selectedCount > 0
                        emphasized: !root.net.working
                        onClicked: root.net.working ? root.net.cancelTask() : root.net.downloadSelected(pv.checked, zip.checked)
                    }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: root.transferVisible
                    value: root.net.progress
                    indeterminate: root.net.busy || root.net.probing || root.net.previewing
                    palette.dark: Theme.colors.accent.primary
                    palette.midlight: Theme.colors.background.controlDisabled
                }
            }
        }
    }
}
