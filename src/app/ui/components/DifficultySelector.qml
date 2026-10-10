pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import MiaCode.UI

Flow {
    id: root

    required property var model
    property int selectedDifficultyId: 0
    signal selected(int difficultyId)

    spacing: 4

    ButtonGroup { id: group }

    Repeater {
        model: root.model
        delegate: AppChoiceButton {
            required property var modelData
            ButtonGroup.group: group
            text: modelData.name
            difficultyId: modelData.id
            checked: root.selectedDifficultyId === modelData.id
            onClicked: root.selected(modelData.id)
        }
    }
}
