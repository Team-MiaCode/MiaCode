import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MiaCode.UI

// The PV batch queue. Progress and cancellation live on the shell's shared overlay.
AppDialog {
    id: root

    required property var mediaTools

    title: qsTrId("qml.batch_compress_videos")
    preferredWidth: 680
    preferredHeight: Theme.dialogHeight
    fillBody: true
    footer: DialogFooter {
        cancelText: qsTrId("qml.back")
        onRejected: root.reject()
    }

    readonly property bool busy: !!root.mediaTools && root.mediaTools.batchRunning
    readonly property var batchJobs: root.mediaTools ? root.mediaTools.batchJobs : []

    body: ColumnLayout {
        spacing: Theme.settingsRowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            AppTextField {
                objectName: "pvBatchDirectoryField"
                Layout.fillWidth: true
                readOnly: true
                text: root.mediaTools ? root.mediaTools.batchDirectory : ""
                placeholderText: qsTrId("qml.choose_a_folder_to_scan")
            }
            AppButton {
                objectName: "pvBatchBrowseButton"
                minimumButtonWidth: 76
                text: qsTrId("action.browse")
                enabled: !root.busy
                onClicked: root.mediaTools.chooseBatchDirectory()
            }
            AppButton {
                objectName: "pvBatchAddButton"
                minimumButtonWidth: 76
                text: qsTrId("qml.add")
                enabled: !root.busy && !!root.mediaTools && root.mediaTools.batchDirectory.length > 0
                onClicked: root.mediaTools.addBatchFolder()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.controlRadius
            color: Theme.surfaceColor(Theme.colors.background.surface)
            border.width: 1 / Screen.devicePixelRatio
            border.color: Theme.colors.border.normal
            clip: true

            ListView {
                objectName: "pvBatchQueue"
                anchors.fill: parent
                anchors.margins: 4
                spacing: 2
                clip: true
                model: root.batchJobs
                ScrollBar.vertical: AppScrollBar {}
                delegate: Item {
                    id: jobRow
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 46

                    HoverHandler { id: rowHover }
                    HoverChrome {
                        anchors.fill: parent
                        fillBounds: true
                        highlightOutset: 0
                        stateColors: Theme.colors.listState
                        hovered: rowHover.hovered
                        showFocusRing: false
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 6
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: jobRow.modelData.displayName
                                elide: Text.ElideMiddle
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                                color: jobRow.modelData.hasVideo ? Theme.colors.text.active : Theme.colors.text.disabled
                            }
                            Text {
                                Layout.fillWidth: true
                                text: jobRow.modelData.status.length > 0 ? jobRow.modelData.status : jobRow.modelData.size
                                elide: Text.ElideRight
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.secondaryFontSize
                            }
                        }
                        IconButton {
                            compact: true
                            iconSource: Qt.resolvedUrl("icons/remove.svg")
                            tooltip: qsTrId("qml.remove")
                            enabled: !root.busy
                            onClicked: root.mediaTools.removeBatchJob(jobRow.index)
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: root.batchJobs.length === 0
                text: qsTrId("qml.choose_a_folder_to_scan")
                horizontalAlignment: Text.AlignHCenter
                color: Theme.colors.text.disabled
                font.family: Theme.uiFont
                font.pixelSize: Theme.secondaryFontSize
                wrapMode: Text.WordWrap
            }
        }

        Text {
            objectName: "pvBatchSummary"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.mediaTools ? root.mediaTools.batchSummary : ""
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.secondaryFontSize
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            AppButton {
                text: qsTrId("media_tools.batch_pv_clear_queue")
                enabled: !root.busy && root.batchJobs.length > 0
                onClicked: root.mediaTools.clearBatchQueue()
            }
            Item { Layout.fillWidth: true }
            AppButton {
                objectName: "pvBatchStartButton"
                text: qsTrId("qml.start_compression")
                emphasized: true
                enabled: !root.busy && root.batchJobs.length > 0
                onClicked: root.mediaTools.startBatchCompression()
            }
        }
    }
}
