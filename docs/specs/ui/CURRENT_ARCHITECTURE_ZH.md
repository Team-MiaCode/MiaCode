# 当前应用架构与所有权

下层库的划分与依赖方向见[模块分层](../architecture/MODULE_LAYERING_CURRENT_ZH.md)。

## 前端、文档与运行时

产品前端由 `src/app/ui/` 的 QML 页面和 C++ model 构成，启动入口为 `Bootstrap`。
QML engine 创建根窗口，`runtime::Session` 是 QObject 装配对象，拥有运行时宿主并附着根窗口。

`ApplicationServices` 持有 `ChartWorkspace`、文件服务、分析服务、编辑同步、UI 请求、任务进度、
通知和预览外观状态，并保存宿主安装的服务槽位。

| 所有者 | 契约 |
| --- | --- |
| ChartWorkspace | 完整文档、difficulty、revision、保存点与 dirty 状态 |
| ChartWorkspaceFileService | 文档读写、编码与原子保存 |
| AnalysisService | 身份与 revision 匹配的分析快照 |
| EditorSyncController | 编辑器上下文、导航与跟随同步 |
| PlaybackCoordinator | 播放控制、播放快照与音频时钟 |
| PreviewHost / TimelineHost | 各自的 surface 入口、投影及命令校验 |
| DocumentSessionHost | 文档会话、文件工作流与页面路由 |
| VideoExportHost / MediaJobsHost | 视频导出与媒体处理引擎 |
| SettingsHost / EditorHost / ValidationHost / StageMediaHost / ShellHost | 偏好、编辑器持久状态、校验展示、舞台媒体、窗口生命周期 |

扩展字段表单提交失败时，`DocumentModel` 保留带文档打开代次的输入草稿及格式错误。
输入校验通过后写入 `ChartWorkspace` 并推进 revision；界面的未保存状态包含草稿，
保存、页面切换和标签/文档关闭均先提交输入，失败时保留当前视图。纠正后原子提交全部扩展字段；
显式放弃或替换文档清除旧草稿。文件服务直接写入分层合并后的文档对象，并以同一对象更新保存点。

具体安装关系以 `src/app/runtime/SessionBootstrap.cpp` 和 `ApplicationServices` 为准。

app 内部依赖方向见[模块分层](../architecture/MODULE_LAYERING_CURRENT_ZH.md#app-内部方向)。
导出页由 UI 层的 `ExportSession` 实现、经 `ExportPagePort` 交给运行时：`Bootstrap` 在装配 Session 前通过
`ApplicationServices::setExportPageFactory` 安装工厂。`DocumentProjection`、`AnalysisProjection`、
`ShortcutRegistry` 与 `PreferenceDocument` 位于 `app/services/`，`WindowParityMetrics` 位于 `app/runtime/shell/`。

## 跨边界数据

- QML model 提交命令，文档与 dirty 状态由 ChartWorkspace 提供。
- 异步分析与导航携带身份和 revision；播放/走带使用相应 generation、revision、sequence 校验。
- Preview、Timeline 和编辑器通过共享播放时钟与同步接口跟随。
- 前端通过 engine/port 调用领域行为；仅 bootstrap 的窗口生命周期允许直接接触 Session。
- 关闭窗口或替换任务时，由所属宿主释放 engine、回调和资源，并清空服务槽位。

## 复用入口

QML 控件、弹层和表单优先复用 `src/app/ui/components/`；主题在 `theme/Theme.qml`，
文案经 Qt Linguist（`translations/*.ts` → 构建目录中的 `.qm` → `/i18n` 内嵌资源），QML 用 `qsTrId`，C++ 用 `qtTrId`；运行期由 `LocaleService` 装载与热切换。偏好持久化在 `app/services/PreferenceDocument`，
文件为用户配置目录下的 `preferences_v2.json`；该文件缺失时只读导入 1.x 的 `preferences.json`
（再缺失时为程序目录旧版 `.miacode_preferences.json`），此后两版各写各的文件；
下层库只经 `common/PreferenceProvider.h` 的端口读写各自的 `app.<section>`，生产实现为
`app/runtime/settings/PreferenceDocumentProvider`。
首次设置由 `SetupWizard` 管理，复用首选项的配色、预览位置、侧栏模式和输入方式；
完成后保存 `ui.setup_completed`，随即进入 `BeginnerGuideOverlay`，引导作为向导的收尾阶段；
“附加”菜单的“启动设置向导”可重新打开向导。
欢迎页的旧版导入提示取自偏好仓库的本次导入结果，配置 schema 与向导完成状态独立。
共享请求/进度复用 UiRequestService 和 JobProgressService；文件与资源解析复用现有领域服务。

整体界面缩放由 `WorkbenchSettings.uiScalePercent` 管理，保存到
`preferences_v2.json` 的 `ui.ui_scale_percent`，默认 100%，范围 75%–200%。
首选项“界面 / 外观”即时调整；主窗口、分离预览和封面窗口复用 `UiScaleSurface`，
按同一倍率缩放布局、文字、图标和所属窗口的 Overlay，布局与弹层位置仍使用基础 UI 单位。
系统窗口坐标与材质区域按倍率转换；字体使用可变换的 Qt 文本渲染，SVG 图标按倍率提高源尺寸。

`UiScaleShortcuts` 通过 `Qt.WindowShortcut` 调整共享缩放比例，快捷键录入期间暂停。

| 动作 | 默认快捷键 | Registry ID |
| --- | --- | --- |
| 放大界面 10 个百分点 | `Ctrl+Alt+=` | `ui.zoom_in` |
| 缩小界面 10 个百分点 | `Ctrl+Alt+-` | `ui.zoom_out` |
| 恢复 100% | `Ctrl+Alt+0` | `ui.zoom_reset` |

macOS 的 `Ctrl` 对应 Command，`Alt` 对应 Option。
`ShortcutRegistry` 读取 `shortcuts.json` 时，将 `editor.font_increase` / `editor.font_decrease` 的自定义按键
映射到 `ui.zoom_in` / `ui.zoom_out`。同一配置节中新 ID 优先，`contextual` 优先于 `actions`，
CWD 文件优先于程序目录文件。编辑动作时清除旧 ID 的 `shortcut` 字段。

主窗口的状态转换与几何记忆由 `WindowChrome` 管理，`Bootstrap` 在显示前恢复、
接受关闭后保存。最小化保留窗口状态中的最大化与全屏标志，系统恢复沿用缩小前状态。
`preferences_v2.json` 的 `ui.main_window` 保存 `normal_geometry`（`x`、`y`、`width`、`height`）、
`screen_name` 和 `maximized`；普通窗口几何在事件循环合并几何与状态变化后采集。
启动恢复普通窗口尺寸与最大化状态，位置按目标屏幕的可用区域约束。
Wayland 的窗口位置由合成器管理，应用恢复尺寸与最大化状态。

仓库地图见 [开发 skill](../../../.agents/skills/miacode-dev-guide/SKILL.md)。
渲染与导出见 [当前渲染契约](../preview/CURRENT_RENDER_EXPORT_CONTRACT_ZH.md)。
