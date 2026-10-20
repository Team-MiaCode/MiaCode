import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI
import "qrc:/intro/qml" as Intro

ColumnLayout {
    id: root

    required property var documentSession
    property real preferredCardHeight: 260
    property int selectedDifficultyId: 0
    readonly property var difficulties: documentSession.difficulties
    readonly property var track: {
        // Read the revision so metadata and chart edits refresh this projection.
        const revision = documentSession.documentRevision
        return visible && selectedDifficultyId > 0
            ? documentSession.difficultyCardTrack(selectedDifficultyId) : ({})
    }
    spacing: Theme.settingsLabelSpacing

    function syncSelection(reset) {
        const preferred = reset ? documentSession.currentDifficultyId : selectedDifficultyId
        for (const option of difficulties) {
            if (option.id === preferred) {
                selectedDifficultyId = preferred
                return
            }
        }
        selectedDifficultyId = difficulties.length > 0 ? difficulties[0].id : 0
    }

    Component.onCompleted: syncSelection(true)
    onDifficultiesChanged: syncSelection(false)
    Connections {
        target: root.documentSession
        function onDocumentReplaced() { root.syncSelection(true) }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.panelPadding

        Label {
            text: qsTrId("cover.difficulty_card")
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.uiFontSize
        }
        AppComboBox {
            objectName: "metadataCardDifficultySelector"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            compact: true
            model: root.difficulties
            textRole: "name"
            valueRole: "id"
            enabled: count > 0
            currentIndex: {
                for (let i = 0; i < root.difficulties.length; ++i) {
                    if (root.difficulties[i].id === root.selectedDifficultyId) return i
                }
                return -1
            }
            Accessible.name: qsTrId("dialog.batch_export.difficulty")
            onActivated: root.selectedDifficultyId = currentValue
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(root.preferredCardHeight, width / cardAspectRatio)
        readonly property var cardTemplate: root.documentSession.difficultyCardTemplate
        readonly property real cardAspectRatio: cardTemplate.card
            ? cardTemplate.card.nativeWidth / (cardTemplate.card.nativeHeight
                - Math.min(0, cardTemplate.layout.mbaseTab.y)) : 420 / 690

        Loader {
            anchors.centerIn: parent
            width: parent.height * parent.cardAspectRatio
            height: parent.height
            active: root.visible && root.selectedDifficultyId > 0
            sourceComponent: Intro.MaimaiBannerCard {
                objectName: "metadataDifficultyCard"
                externalTemplate: root.documentSession.difficultyCardTemplate
                fitToContent: true
                trackOverrides: root.track
                jacketImage: root.documentSession.metadataCoverSource
                cacheDynamicImages: false
            }
        }
        Label {
            anchors.fill: parent
            visible: root.selectedDifficultyId === 0
            text: qsTrId("metadata.no_difficulty_card")
            color: Theme.colors.text.secondary
            font.family: Theme.uiFont
            font.pixelSize: Theme.secondaryFontSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
