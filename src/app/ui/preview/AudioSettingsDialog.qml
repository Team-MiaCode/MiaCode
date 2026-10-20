import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// 音频设置. Every change writes straight through and is applied to the running
// preview, matching the Widgets dialog: there is no OK/Apply.
//
// Edits also audition the channel they touched, so a level can be judged without
// starting playback. The model owns the sample runtime and the settle delay; the
// page only has to say when a handle is down (audition waits for the release)
// and when the panel is gone (the runtime goes with it).
AppDialog {
    id: root

    required property var audioSettings

    // The channel set is fixed, so the Repeater is driven by the key list and
    // never resets. Levels arrive separately, through `levels`: feeding the
    // Repeater the value rows instead would rebuild every delegate on every
    // change — including the slider under the user's finger, which ends the drag.
    property var keys: []
    property var levels: ({})

    // Keep every row's label edge aligned while allowing localized labels to
    // size the column from their actual font metrics.
    readonly property FontMetrics labelMetrics: FontMetrics {
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
    }
    readonly property real labelColumnWidth: {
        var widest = 0;
        for (var i = 0; i < root.keys.length; ++i) {
            var channel = root.levels[root.keys[i]];
            if (channel)
                widest = Math.max(widest, root.labelMetrics.advanceWidth(channel.label));
        }
        return Math.ceil(widest) + 4;
    }

    function refresh() {
        var rows = root.audioSettings.channels();
        var byKey = {};
        var keyList = [];
        for (var i = 0; i < rows.length; ++i) {
            byKey[rows[i].key] = rows[i];
            keyList.push(rows[i].key);
        }
        // Levels first: the delegates read them the moment the key list appears.
        root.levels = byKey;
        if (root.keys.length !== keyList.length) {
            root.keys = keyList;
        }
    }

    onAboutToShow: root.refresh()
    onClosed: root.audioSettings.releaseAudition()

    Connections {
        target: root.audioSettings
        function onChanged() {
            root.refresh();
        }
    }

    title: qsTrId("action.audio_settings")
    preferredWidth: 560
    preferredHeight: dialogContent.implicitHeight + topPadding + defaultActions.implicitHeight
    leftPadding: Theme.dialogPadding
    rightPadding: Theme.dialogPadding
    topPadding: 24
    bottomPadding: 0
    header: null
    footer: DialogFooter {
        id: defaultActions
        choices: [
            { id: "save_default", label: qsTrId("qml.set_as_local_default") },
            { id: "restore_default", label: qsTrId("qml.restore_local_defaults") }
        ]
        onChosen: function(choiceId) {
            if (choiceId === "save_default") root.audioSettings.saveAsSoftwareDefault()
            else if (choiceId === "restore_default") root.audioSettings.restoreSoftwareDefault()
        }
    }

    contentItem: ColumnLayout {
        id: dialogContent
        spacing: 2 * Theme.settingsRowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.settingsRowSpacing

            Text {
                Layout.fillWidth: true
                text: root.title
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize + 8
                font.weight: Font.DemiBold
                color: Theme.colors.text.heading
                elide: Text.ElideRight
            }
            IconButton {
                objectName: "audioSettingsCloseButton"
                iconSource: Qt.resolvedUrl("icons/close.svg")
                tooltip: qsTrId("action.close")
                onClicked: root.reject()
            }
        }

        ScrollView {
            id: pageViewport
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: channelColumn.implicitHeight
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: AppScrollBar {}

            ColumnLayout {
                id: channelColumn
                width: pageViewport.availableWidth
                spacing: Theme.settingsRowSpacing

                Repeater {
                    model: root.keys
                    delegate: RowLayout {
                        id: channelRow
                        required property string modelData
                        readonly property var channel: root.levels[modelData]
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.minimumWidth: root.labelColumnWidth
                            Layout.preferredWidth: root.labelColumnWidth
                            Layout.maximumWidth: root.labelColumnWidth
                            text: channelRow.channel.label
                            color: Theme.colors.text.secondary
                            font.family: Theme.uiFont
                            font.pixelSize: Theme.uiFontSize
                        }
                        IconButton {
                            iconSource: Qt.resolvedUrl(channelRow.channel.muted ? "icons/volume-x.svg" : "icons/volume-2.svg")
                            iconWidth: 18
                            iconHeight: 18
                            tooltip: channelRow.channel.muted ? qsTrId("qml.unmute") : qsTrId("qml.mute")
                            onClicked: root.audioSettings.toggleChannelMuted(channelRow.modelData)
                        }
                        AppSlider {
                            id: levelSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            stepSize: 1
                            onMoved: root.audioSettings.setChannelPercent(channelRow.modelData, Math.round(value))
                            onPressedChanged: root.audioSettings.setAuditionHeld(pressed)

                            // Dragging writes `value` imperatively, which would kill a
                            // plain binding for good — after one drag the handle would
                            // stop answering 恢复本地默认. A Binding re-applies instead,
                            // and stands down while the handle is held so it cannot
                            // fight the drag.
                            Binding {
                                target: levelSlider
                                property: "value"
                                value: channelRow.channel.percent
                                when: !levelSlider.pressed
                                restoreMode: Binding.RestoreNone
                            }
                        }
                        // Double-clicking the percentage types it. A short handle
                        // cannot land on an exact number, and these are numbers people
                        // want exactly.
                        EditableValue {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 22
                            text: channelRow.channel.percent + "%"
                            value: channelRow.channel.percent
                            from: 0
                            to: 100
                            onCommitted: function (v) {
                                root.audioSettings.setChannelPercent(channelRow.modelData, Math.round(v));
                            }
                        }
                    }
                }

                AppSwitch {
                    objectName: "audioBreakSlideTailCheerSwitch"
                    text: qsTrId("qml.mute_break_star_tail_judgement_sound")
                    checked: root.audioSettings.breakSlideTailCheerMuted
                    onToggled: root.audioSettings.breakSlideTailCheerMuted = checked
                }

            }
        }
    }
}
