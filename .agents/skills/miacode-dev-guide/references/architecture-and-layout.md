# 仓库与复用地图

路径均相对仓库根目录。表格提供查找起点，不是所有类的清单；用 `rg` 在所属模块内继续定位。

## 模块和所有权

| 任务 | 入口 | 复用与边界 |
| --- | --- | --- |
| 启动、窗口与装配 | `src/app/main.cpp`、`src/app/ui/Bootstrap.cpp`、`src/app/runtime/` | Session 拥有宿主；QML 引擎创建根窗口 |
| 安卓 P0/P1 原型 | `src/android/`、`packaging/android/`、`scripts/build/build-android.ps1` | 独立 CMake 入口；文档权威复用 ChartWorkspace，SAF/JNI 与恢复由 AndroidDocumentSession/AndroidPlatformBridge 接入；媒体探针不能替代 v2 渲染一致性验收，进度见 `docs/specs/android/ANDROID_P0_P1_IMPLEMENTATION_ZH.md` |
| 文档、打开保存、分析 | `src/app/services/` | ChartWorkspace、ChartWorkspaceFileService、AnalysisService；不要在 QML model 复制文档权威 |
| 编辑器与书签 | `src/app/ui/editor/`、`src/editor/` | EditorController/InputBridge、EditorSyncController；文本策略与书签语法复用 src/editor |
| 播放、走带与同步 | `src/app/runtime/playback/`、`src/app/services/PlaybackControl.h` | PlaybackCoordinator 是播放权威；Preview/Timeline 通过端口和带身份的快照交互 |
| 时间轴 | `src/timeline/`、`src/app/runtime/timeline/` | 模型、坐标和 QSG 表面；复用已有 geometry/state bridge |
| 预览 | `src/preview/runtime/`、`src/preview/quick_scene/` | PreviewRuntime、场景缓存与 QSG 图层 |
| 解析与变换 | `src/core/chart/` | document、parser、transform；避免在 UI 重写 simai 语法 |
| 场景数学 | `src/core/scene/` | Frame/layer state、draw order、skin selectors，供实时预览和离线渲染共享 |
| 音频 | `src/audio/` | PreviewAudioBackend、QtPreviewSfxRuntime；设备和 BASS/miniaudio 访问留在此边界 |
| 视频/封面导出 | `src/app/ui/export/`、`src/app/runtime/export/`、`src/tools/video_export/`、`src/tools/cover_export/` | UI session、ExportEngine、snapshot/worker、合成器各负其责 |
| 延迟、无理、媒体工具 | `src/tools/latency/`、`src/tools/muri/`、`src/tools/media/` | 前端经 `ApplicationServices` 的 engine 槽位调用；不要复制分析实现 |
| 配置、资源、日志 | `src/common/` | 共享配置与纯 helper；不要把单一功能私有状态放进 common |

装配细节查 `src/app/runtime/ASSEMBLY.md` 和 `ApplicationServices` 的实际安装点；其中的阶段计划不代表代码已实现。

Android 校准页复用 `LatencyModel`、`LatencyPage`、`LatencyAnalysis` 和测试谱面生成器。
`LatencyEngine` 的试听端口为 `LatencyAudition`；桌面控制器与 Android `MobileLatency`
分别连接各自现有播放权威。Android 测试谱面仅覆盖 `MobilePreview` 的预览数据源，
`MobileTimeline` 读取同一数据源；用户正文仍归 `AndroidDocumentSession` 的 ChartWorkspace。

Android 的自定义片头音源库由 `MobilePreferencesStore` 在启动时设置到偏好目录旁的
`music` 私有目录；`PreviewSfxAssets` 是导入、试听与离线导出的共同路径解析入口。
`MobileExportComposition` 连接共享导出页与片头场景，`MobilePreview` 持有传输，
`MobileSfxOutput` 负责 PCM 输出及可替换的片头解码；音源回调必须匹配当前代次。
历史中的 MainWindow、v1 QuickShell 外壳、DComp 渲染器和外置预览 worker 已退役，不能作为新功能入口。部分类型名仍含 QuickShell，不等于有第二套产品前端。

## QML 可复用组件

共享组件位于 `src/app/ui/components/`，优先查看相邻页面的用法。

| 需求 | 优先复用 |
| --- | --- |
| 设置表单 | LabeledCombo、LabeledSlider；底层 AppComboBox、AppSlider、AppSwitch、AppCheckBox |
| 按钮、输入与菜单 | AppButton、IconButton、AppTextField、AppTextArea、AppMenu/AppMenuItem |
| 对话框与提示 | AppDialog、DialogFooter、ChoiceDialog；请求接 UiRequestService / UiRequestHost |
| 进度、浮层与公共视觉 | JobProgressService / JobProgressOverlay、FloatingCard、HoverChrome、AppDropdownPanel |
| 颜色、间距、字体与背景 | `src/app/ui/theme/Theme.qml`；C++ 主题在同目录 `UiTheme` |
| 用户可见文案 | `qsTrId` / `qtTrId`；目录是 `translations/{en_US,zh_CN,ja_JP}.ts`，运行期由 `LocaleService` 加载 |
| 语言、主题与 preferences.json | `src/app/ui/preferences/PreferenceDocument.h` |

例如增加设置选择器：从 `src/app/ui/preferences/PreferencesDialog.qml` 的
LabeledCombo 用法出发，经过 `PreferencesModel`、`PreferenceDocument` 与 `miacode::PreferencesStore` 接入持久化。
偏好设置由 settings host 管理，谱面修改才进入 ChartWorkspace。
布局尺寸、弹层生命周期、滚动和键盘行为沿用组件契约，不在业务页复制 chrome。
