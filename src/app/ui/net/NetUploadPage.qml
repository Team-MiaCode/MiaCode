pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    required property var uploadModel
    property bool logVisible: false
    readonly property bool transferVisible: uploadModel.busy || uploadModel.progress > 0

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
                title: qsTrId("net.ui.upload_page")
                sidebarTitle: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                enabled: !root.uploadModel.busy

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.uploadModel.loggedIn
                    Caption {
                        Layout.fillWidth: true
                        text: qsTrId("net.ui.logged_in").arg(root.uploadModel.username)
                    }
                    NetPageButton {
                        visible: root.uploadModel.loggedIn
                        checkable: false
                        text: qsTrId("net.ui.logout")
                        onClicked: root.uploadModel.logout()
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    visible: !root.uploadModel.loggedIn
                    columns: 2
                    uniformCellWidths: true
                    columnSpacing: 12

                    NetFormField {
                        label: qsTrId("net.ui.username")
                        AppTextField {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: root.uploadModel.username
                            outlined: true
                            Accessible.name: qsTrId("net.ui.username")
                            onTextEdited: root.uploadModel.username = text
                        }
                    }
                    NetFormField {
                        label: qsTrId("net.ui.password")
                        AppTextField {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: root.uploadModel.password
                            echoMode: TextInput.Password
                            outlined: true
                            Accessible.name: qsTrId("net.ui.password")
                            onTextEdited: root.uploadModel.password = text
                            onAccepted: root.uploadModel.login()
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: !root.uploadModel.loggedIn
                    AppCheckBox {
                        text: qsTrId("net.ui.remember_account")
                        checked: root.uploadModel.remember
                        enabled: root.uploadModel.secureStorageAvailable
                        onClicked: root.uploadModel.remember = checked
                    }
                    Item { Layout.fillWidth: true }
                    NetPageButton {
                        text: qsTrId("net.ui.login")
                        onClicked: root.uploadModel.login()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: Theme.separatorColor
            }

            NetFormField {
                label: qsTrId("net.ui.chart_root")
                enabled: !root.uploadModel.busy
                AppTextField {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.uploadModel.rootDirectory
                    outlined: true
                    Accessible.name: qsTrId("net.ui.chart_root")
                    onEditingFinished: root.uploadModel.rootDirectory = text
                }
                NetPageButton {
                    iconSource: Qt.resolvedUrl("icons/folder-open.svg")
                    tooltip: qsTrId("net.ui.browse")
                    onClicked: root.uploadModel.browse()
                }
                NetPageButton {
                    text: qsTrId("net.ui.add")
                    onClicked: root.uploadModel.addDirectory()
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
                        text: qsTrId("net.ui.queue_count").arg(root.uploadModel.resultCount)
                    }
                    NetPageButton {
                        iconSource: Qt.resolvedUrl("icons/arrow-up.svg")
                        tooltip: qsTrId("net.ui.move_up")
                        enabled: !root.uploadModel.busy && root.uploadModel.resultCount > 0
                        onClicked: root.uploadModel.moveSelected(-1)
                    }
                    NetPageButton {
                        iconSource: Qt.resolvedUrl("icons/arrow-down.svg")
                        tooltip: qsTrId("net.ui.move_down")
                        enabled: !root.uploadModel.busy && root.uploadModel.resultCount > 0
                        onClicked: root.uploadModel.moveSelected(1)
                    }
                }

                NetResultTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 180
                    model: root.uploadModel
                    interactive: !root.uploadModel.busy
                    reorderable: true
                    wrappingRole: "fileNames"
                    wrappingColumn: 2
                    columns: [
                        {title: "#", width: 24},
                        {title: qsTrId("net.ui.chart_folder"), role: "displayName", tooltipRole: "path", weight: 50},
                        {title: qsTrId("net.ui.files"), role: "fileNames", weight: 35, minimum: 80, wrap: true},
                        {title: qsTrId("net.ui.state"), role: "itemState", width: 88, alignment: Text.AlignHCenter}
                    ]
                    onRowActivated: row => root.uploadModel.selectRow(row)
                    onRowDragStarted: row => root.uploadModel.startRowDrag(row)
                    onRowsDropped: row => root.uploadModel.moveSelectedTo(row)

                    Caption {
                        anchors.centerIn: parent
                        width: Math.max(0, parent.width - 48)
                        visible: root.uploadModel.resultCount === 0
                        text: qsTrId("net.ui.queue_hint")
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
                    text: root.uploadModel.logText
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
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    NetPageButton {
                        checkable: false
                        text: qsTrId("net.ui.remove_selected")
                        enabled: !root.uploadModel.busy && root.uploadModel.resultCount > 0
                        onClicked: root.uploadModel.removeSelected()
                    }
                    NetPageButton {
                        checkable: false
                        text: qsTrId("net.ui.clear_queue")
                        enabled: !root.uploadModel.busy && root.uploadModel.resultCount > 0
                        onClicked: root.uploadModel.clearQueue()
                    }
                    Item { Layout.fillWidth: true }
                    NetPageButton {
                        text: qsTrId("net.ui.view_log")
                        checkable: true
                        checked: root.logVisible
                        onClicked: root.logVisible = checked
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Caption {
                        Layout.fillWidth: true
                        text: root.uploadModel.statusText
                            || (root.transferVisible ? Math.round(root.uploadModel.progress * 100) + "%" : "")
                    }
                    NetPageButton {
                        text: root.uploadModel.busy ? qsTrId("net.ui.cancel_task")
                            : root.uploadModel.retryAvailable ? qsTrId("net.upload_retry_failed") : qsTrId("net.ui.upload_queue")
                        emphasized: !root.uploadModel.busy
                        enabled: root.uploadModel.busy || root.uploadModel.resultCount > 0
                        onClicked: root.uploadModel.busy ? root.uploadModel.cancel() : root.uploadModel.upload()
                    }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: root.transferVisible
                    value: root.uploadModel.progress
                    indeterminate: root.uploadModel.busy && root.uploadModel.resultCount === 0
                    palette.dark: Theme.colors.accent.primary
                    palette.midlight: Theme.colors.background.controlDisabled
                }
            }
        }
    }
}
