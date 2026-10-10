import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

// 预览设置. Like 音频设置 and 偏好设置, every control writes straight through and
// persists on the spot — there is no OK/Apply.
//
// 视频、玩法和皮肤 all describe the running preview. 性能只含预览刷新率，仍放在
// 偏好设置 → 性能，避免同一设置有两个入口。
AppDialog {
    id: root

    required property var previewSettings

    title: qsTrId("action.video_settings")
    preferredWidth: 640
    preferredHeight: Theme.dialogHeight
    footer: DialogFooter {
        cancelText: qsTrId("action.close")
        onRejected: root.reject()
    }

    body: ColumnLayout {
        spacing: Theme.settingsRowSpacing

        AppTabBar {
            id: settingsTabs
            Layout.fillWidth: true
            tabs: root.previewSettings
                  ? [root.previewSettings.videoGroupLabel,
                     root.previewSettings.gameplayGroupLabel,
                     root.previewSettings.skinGroupLabel] : []
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.colors.border.normal
        }

        PreviewAppearancePages {
            Layout.fillWidth: true
            previewSettings: root.previewSettings
            currentIndex: settingsTabs.currentIndex
        }
    }

    onOpened: if (root.previewSettings) root.previewSettings.refresh()
}
