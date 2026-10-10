# 仓库与复用地图

路径均相对仓库根目录。用 `rg` 在所属模块内查找具体实现。

## 模块和所有权

| 任务 | 入口 | 复用与边界 |
| --- | --- | --- |
| 启动、窗口与装配 | `src/app/main.cpp`、`src/app/ui/Bootstrap.cpp`、`src/app/runtime/` | Session 拥有宿主；QML 引擎创建根窗口 |
| 文档、打开保存、分析 | `src/app/services/` | ChartWorkspace、ChartWorkspaceFileService、AnalysisService；不要在 QML model 复制文档权威 |
| 编辑器与书签 | `src/app/ui/editor/`、`src/editor/`、`third_party/ScintillaQuick/` | SourceEditor 的 ScintillaEditor 管理正文与行号；EditorController 和 EditorSyncController 接入编辑策略与同步，文本策略与书签语法复用 src/editor |
| 播放、走带与同步 | `src/app/runtime/playback/`、`src/app/services/PlaybackControl.h` | PlaybackCoordinator 是播放权威；Preview/Timeline 通过端口和带身份的快照交互 |
| 时间轴 | `src/timeline/`、`src/timeline/quick/`、`src/app/runtime/timeline/` | 模型、坐标和 QSG 表面（QML 模块 `MiaCode.Timeline`）；复用已有 geometry/state bridge |
| 预览 | `src/preview/runtime/`、`src/preview/quick_scene/`、`src/preview/stage_media/` | PreviewRuntime、场景缓存与 QSG 图层（QML 模块 `MiaCode.Preview`）；PV/BG 解码在 stage_media |
| 解析与变换 | `src/core/chart/` | document、parser、transform 与音符模型 `model/`；避免在 UI 重写 simai 语法 |
| 场景数学 | `src/core/scene/`、`src/core/video/` | Frame/layer state、draw order、skin selectors、预览配置与 SFX 语义，供实时预览和离线渲染共享 |
| 音频 | `src/audio/`、`src/audio/bass/` | PreviewAudioBackend、QtPreviewSfxRuntime、WaveformCache；BASS 后端与解码器经注入提供 |
| 视频/封面导出 | `src/app/ui/export/`、`src/app/runtime/export/`、`src/export/` | UI session（经 ExportPagePort 交给运行时）、ExportEngine、snapshot/worker、合成器与导出会话各负其责 |
| 延迟、无理、媒体工具 | `src/app/runtime/latency/`、`src/core/analysis/`、`src/media_tools/` | 前端经 `ApplicationServices` 的 engine 槽位调用；不要复制分析实现 |
| 命令行诊断 | `src/devtools/`、`cmake/devtools/CliTools.cmake` | 开发程序入口集中于 devtools；领域算法链接所属库 |
| 日志、诊断与偏好端口 | `src/common/` | DebugLog、PreferenceProvider 等基础设施与纯 helper；领域配置归所属模块 |
| 偏好文档与存储 | `src/app/services/`、`src/app/runtime/settings/` | PreferenceDocument、PreferencesStore 和 PreferenceDocumentProvider；设置宿主负责装配与持久化 |
| 库与依赖方向 | `cmake/MiaCodeModules.cmake`、`docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md` | 一个目录属于一个库；跨库改动后运行 `scripts/governance/module_layering.py` |

装配细节查 `src/app/runtime/SessionBootstrap.cpp` 和 `ApplicationServices` 的实际安装点。


## QML 可复用组件

通用组件位于 `src/app/ui/components/`，窗口装饰位于 `src/app/ui/chrome/`，优先查看相邻页面的用法。

| 需求 | 优先复用 |
| --- | --- |
| 设置表单 | LabeledCombo、LabeledSlider；底层 AppComboBox、AppSlider、AppSwitch、AppCheckBox |
| 按钮、输入与菜单 | AppButton、IconButton、AppTextField、AppTextArea、AppMenu/AppMenuItem |
| 对话框与提示 | AppDialog、DialogFooter、ChoiceDialog；请求接 UiRequestService / UiRequestHost |
| 进度、浮层与公共视觉 | JobProgressService / JobProgressOverlay、FloatingCard、HoverChrome、AppDropdownPanel |
| 窗口标题栏、拖动与状态 | WindowChrome、WindowTitleBar、WindowGestureArea；MainSplitView 管理预览分离与合入 |
| 界面图标与按钮状态 | `src/app/ui/resources/icons/`；IconButton 的 iconSource/filledIconSource，选型与自绘规则见 `qt-ui-design` |
| 颜色、间距、字体与背景 | `src/app/ui/theme/Theme.qml`；C++ 主题在同目录 `UiTheme` |
| 用户可见文案 | `qsTrId` / `qtTrId`；目录是 `translations/` 中的 en_US、zh_CN、ja_JP TS 文件，运行期由 `LocaleService` 加载 |
| 语言、主题与 preferences.json | `src/app/services/PreferenceDocument.h` |

例如增加设置选择器：从 `src/app/ui/preferences/PreferencesDialog.qml` 的
LabeledCombo 用法出发，经过 `PreferencesModel`、`PreferenceDocument` 与 `miacode::PreferencesStore` 接入持久化。
偏好设置由 settings host 管理，谱面修改才进入 ChartWorkspace。下层库不读 `preferences.json`：
它们经 `src/common/PreferenceProvider.h` 读写自己的 `app.<section>`，由 `PreferenceDocumentProvider` 持久化。
布局尺寸、弹层生命周期、滚动和键盘行为沿用组件契约，不在业务页复制 chrome。
