pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root
    required property var net
    signal closeRequested()
    property bool logVisible: false
    function runQuery() {
        net.query({uploader: uploader.text, tag: tag.text, title: song.text,
            startDate: startDate.text, endDate: endDate.text, caseSensitive: !fuzzy.checked,
            forceRefresh: refresh.checked, sort: sorting.currentValue})
    }
    component Caption: Label { color: Theme.colors.text.active; font.family: Theme.uiFont; font.pixelSize: Theme.uiFontSize }
    component FieldGroup: RowLayout {
        id: group
        property string label
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 8
        Caption { text: group.label }
    }
    Flickable {
        id: page
        anchors.fill: parent
        contentWidth: width
        contentHeight: body.height + 32
        flickableDirection: Flickable.VerticalFlick
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar { id: pageScrollBar }
        ColumnLayout {
            id: body
            x: 18
            y: 16
            width: Math.max(0, page.width - 36 - pageScrollBar.implicitWidth)
            height: Math.max(page.height - 32, implicitHeight)
            spacing: 10
            GridLayout {
                id: filterForm
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: Math.max(1, Math.min(6, Math.floor((width + columnSpacing) / (190 + columnSpacing))))
                columnSpacing: 10
                rowSpacing: 8
                enabled: !root.net.working
                FieldGroup {
                    label: qsTrId("net.ui.user_id")
                    AppTextField { id: uploader; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 110; Accessible.name: qsTrId("net.ui.user_id") }
                }
                FieldGroup {
                    label: "Tag"
                    AppTextField { id: tag; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 110; Accessible.name: "Tag" }
                }
                FieldGroup {
                    label: qsTrId("net.ui.song_title")
                    AppTextField { id: song; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 140; Accessible.name: qsTrId("net.ui.song_title") }
                }
                FieldGroup {
                    label: qsTrId("net.start")
                    AppDateField { id: startDate; Layout.fillWidth: true; Layout.minimumWidth: 0; selectedDate: new Date(new Date().getFullYear(), new Date().getMonth() - 1, new Date().getDate()) }
                }
                FieldGroup {
                    label: qsTrId("net.ui.end")
                    AppDateField { id: endDate; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("net.ui.query"); onClicked: root.runQuery() }
            }
            GridLayout {
                id: outputForm
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: width >= 600 ? 4 : 2
                columnSpacing: 10
                rowSpacing: 8
                enabled: !root.net.working
                Caption { text: qsTrId("net.output_directory") }
                AppTextField { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.net.outputDirectory; Accessible.name: qsTrId("net.output_directory"); onEditingFinished: root.net.outputDirectory = text }
                AppButton { text: qsTrId("net.ui.browse"); onClicked: root.net.browseOutputDirectory() }
                AppCheckBox { id: zip; Layout.minimumWidth: 0; text: qsTrId("net.ui.create_zip") }
            }
            Flow {
                id: options
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 10
                enabled: !root.net.working
                RowLayout {
                    width: Math.min(options.width, implicitWidth)
                    spacing: 8
                    Caption { text: qsTrId("net.ui.sort") }
                    AppComboBox {
                        id: sorting
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 220
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
                            {label: qsTrId("net.sort_status_descending"), value: "status_desc"}]
                    }
                }
                RowLayout {
                    width: Math.min(options.width, implicitWidth)
                    spacing: 10
                    AppButton { text: qsTrId("net.ui.probe"); onClicked: root.net.probe() }
                    Caption { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 150; text: root.net.connectionText || qsTrId("net.ui.untested"); elide: Text.ElideRight }
                }
                AppCheckBox { id: fuzzy; checked: true; text: qsTrId("net.ui.fuzzy") }
                AppCheckBox { id: refresh; text: qsTrId("net.ui.force_refresh") }
                AppCheckBox { id: pv; checked: true; text: qsTrId("net.download_pv") }
            }
            NetResultTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: Theme.controlMinHeight * 3
                model: root.net
                interactive: !root.net.working
                columns: [{title: "", width: 36}, {title: qsTrId("net.select"), width: 58, checkbox: true},
                    {title: qsTrId("net.title"), role: "title", weight: 22}, {title: qsTrId("net.ui.artist"), role: "artist", weight: 15, wrap: true},
                    {title: qsTrId("net.designer"), role: "designer", weight: 18, wrap: true}, {title: qsTrId("qml.level"), role: "levels", width: 88},
                    {title: qsTrId("net.ui.uploaded_at"), role: "uploadedAt", width: 170}, {title: qsTrId("net.ui.state"), role: "itemState", weight: 12},
                    {title: qsTrId("net.online_preview"), width: 94, preview: true}]
                onSelectionChanged: (chartId, selected) => root.net.setSelected(chartId, selected)
                onPreviewRequested: chartId => root.net.preview(chartId)
                Caption { anchors.centerIn: parent; visible: root.net.resultCount === 0; text: qsTrId("net.ui.query_hint"); color: Theme.colors.text.secondary }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: 80
                Layout.preferredHeight: Math.max(80, Math.min(160, page.height * 0.2))
                visible: root.logVisible
                clip: true
                TextArea { text: root.net.logText; readOnly: true; selectByMouse: true; wrapMode: Text.Wrap; color: Theme.colors.text.primary; font.family: Theme.uiFont; background: Rectangle { color: Theme.colors.background.control; border.color: Theme.colors.border.control } }
            }
            GridLayout {
                id: selectionActions
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: width >= 650 ? 4 : 3
                columnSpacing: 10
                rowSpacing: 8
                Caption { Layout.columnSpan: selectionActions.columns === 4 ? 1 : 3; Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.net.errorText || root.net.statusText || qsTrId("net.ui.query_hint"); elide: Text.ElideRight }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("net.select_all"); enabled: !root.net.working && root.net.resultCount > 0; onClicked: root.net.selectAll(true) }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("net.ui.clear_selection"); enabled: !root.net.working && root.net.resultCount > 0; onClicked: root.net.selectAll(false) }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("net.ui.view_log"); selected: root.logVisible; onClicked: root.logVisible = !root.logVisible }
            }
            GridLayout {
                id: transferActions
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: width >= 650 ? (root.net.paused ? 4 : 3) : 2
                columnSpacing: 10
                rowSpacing: 8
                Item {
                    Layout.columnSpan: transferActions.columns === 2 ? 2 : 1
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    implicitHeight: Theme.controlMinHeight
                    ProgressBar { anchors.fill: parent; from: 0; to: 1; value: root.net.progress; indeterminate: root.net.busy || root.net.probing || root.net.previewing }
                    Caption { anchors.fill: parent; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: root.net.progressText || Math.round(root.net.progress * 100) + "%" }
                }
                AppButton { Layout.columnSpan: transferActions.columns === 2 ? 2 : 1; Layout.fillWidth: true; Layout.minimumWidth: 0; visible: root.net.paused; text: qsTrId("net.ui.resume"); onClicked: root.net.resumeDownload() }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 220; text: root.net.working ? qsTrId("net.ui.cancel_task") : qsTrId("net.download_selected"); enabled: root.net.working || root.net.selectedCount > 0; emphasized: !root.net.working; onClicked: root.net.working ? root.net.cancelTask() : root.net.downloadSelected(pv.checked, zip.checked) }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("action.close"); onClicked: root.closeRequested() }
            }
        }
    }
}
