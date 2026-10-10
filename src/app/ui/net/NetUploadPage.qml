pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI
Item {
    id: root
    required property var uploadModel
    signal closeRequested()
    property bool logVisible: false
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
            x: 18; y: 16
            width: Math.max(0, page.width - 36 - pageScrollBar.implicitWidth)
            height: Math.max(page.height - 32, implicitHeight)
            spacing: 12
            GridLayout {
                id: accountForm
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: width >= 900 ? 4 : width >= 540 ? 2 : 1
                columnSpacing: 10
                rowSpacing: 10
                enabled: !root.uploadModel.busy
                FieldGroup {
                    label: qsTrId("net.ui.username")
                    AppTextField { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 240; text: root.uploadModel.username; Accessible.name: qsTrId("net.ui.username"); onTextEdited: root.uploadModel.username = text }
                }
                FieldGroup {
                    label: qsTrId("net.ui.password")
                    AppTextField { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 240; text: root.uploadModel.password; echoMode: TextInput.Password; Accessible.name: qsTrId("net.ui.password"); onTextEdited: root.uploadModel.password = text }
                }
                AppCheckBox { text: qsTrId("net.ui.remember_account"); checked: root.uploadModel.remember; enabled: root.uploadModel.secureStorageAvailable; onClicked: root.uploadModel.remember = checked }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.uploadModel.loggedIn ? qsTrId("net.ui.logout") : qsTrId("net.ui.login"); onClicked: root.uploadModel.loggedIn ? root.uploadModel.logout() : root.uploadModel.login() }
            }
            GridLayout {
                id: directoryForm
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: width >= 650 ? 4 : 2
                columnSpacing: 10
                rowSpacing: 10
                enabled: !root.uploadModel.busy
                Caption { text: qsTrId("net.ui.chart_root") }
                AppTextField { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.uploadModel.rootDirectory; Accessible.name: qsTrId("net.ui.chart_root"); onEditingFinished: root.uploadModel.rootDirectory = text }
                AppButton { Layout.fillWidth: directoryForm.columns === 2; Layout.minimumWidth: 0; text: qsTrId("net.ui.browse"); onClicked: root.uploadModel.browse() }
                AppButton { Layout.fillWidth: directoryForm.columns === 2; Layout.minimumWidth: 0; text: qsTrId("net.ui.add"); onClicked: root.uploadModel.addDirectory() }
            }
            NetResultTable {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0; Layout.minimumHeight: Theme.controlMinHeight * 3
                model: root.uploadModel; interactive: !root.uploadModel.busy
                reorderable: true
                wrappingRole: "fileNames"; wrappingColumn: 2
                columns: [{title: "", width: 36}, {title: qsTrId("net.ui.chart_folder"), role: "displayName", weight: 20},
                    {title: qsTrId("net.ui.files"), role: "fileNames", weight: 32, wrap: true}, {title: qsTrId("net.ui.path"), role: "path", weight: 33}, {title: qsTrId("net.ui.state"), role: "itemState", weight: 15}]
                onRowActivated: row => root.uploadModel.selectRow(row)
                onRowDragStarted: row => root.uploadModel.startRowDrag(row)
                onRowsDropped: row => root.uploadModel.moveSelectedTo(row)
            }
            ScrollView {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.minimumHeight: 80; Layout.preferredHeight: Math.max(80, Math.min(160, page.height * 0.2)); visible: root.logVisible; clip: true
                TextArea { text: root.uploadModel.logText; readOnly: true; selectByMouse: true; wrapMode: Text.Wrap; color: Theme.colors.text.primary; font.family: Theme.uiFont; background: Rectangle { color: Theme.colors.background.control; border.color: Theme.colors.border.control } }
            }
            GridLayout {
                id: queueActions
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: width >= 750 ? 6 : 3
                columnSpacing: 10
                rowSpacing: 8
                Caption { Layout.columnSpan: queueActions.columns === 6 ? 1 : 3; Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.uploadModel.statusText || qsTrId("net.ui.queue_count").arg(root.uploadModel.resultCount); elide: Text.ElideRight }
                IconButton { glyph: "↑"; tooltip: qsTrId("net.ui.move_up"); enabled: !root.uploadModel.busy; onClicked: root.uploadModel.moveSelected(-1) }
                IconButton { glyph: "↓"; tooltip: qsTrId("net.ui.move_down"); enabled: !root.uploadModel.busy; onClicked: root.uploadModel.moveSelected(1) }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("net.ui.view_log"); selected: root.logVisible; onClicked: root.logVisible = !root.logVisible }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("net.ui.remove_selected"); enabled: !root.uploadModel.busy; onClicked: root.uploadModel.removeSelected() }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("net.ui.clear_queue"); enabled: !root.uploadModel.busy; onClicked: root.uploadModel.clearQueue() }
            }
            GridLayout {
                id: transferActions
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                columns: width >= 650 ? 3 : 2
                columnSpacing: 10
                rowSpacing: 8
                Item { Layout.columnSpan: transferActions.columns === 2 ? 2 : 1; Layout.fillWidth: true; Layout.minimumWidth: 0; implicitHeight: Theme.controlMinHeight
                    ProgressBar { anchors.fill: parent; value: root.uploadModel.progress; indeterminate: root.uploadModel.busy && root.uploadModel.resultCount === 0 }
                    Caption { anchors.centerIn: parent; text: Math.round(root.uploadModel.progress * 100) + "%" }
                }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 220; text: root.uploadModel.busy ? qsTrId("net.ui.cancel_task") : root.uploadModel.retryAvailable ? qsTrId("net.upload_retry_failed") : qsTrId("net.ui.upload_queue"); emphasized: !root.uploadModel.busy; enabled: root.uploadModel.busy || root.uploadModel.resultCount > 0; onClicked: root.uploadModel.busy ? root.uploadModel.cancel() : root.uploadModel.upload() }
                AppButton { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTrId("action.close"); onClicked: root.closeRequested() }
            }
        }
    }
}
