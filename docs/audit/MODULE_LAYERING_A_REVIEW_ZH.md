---
lifecycle: working
---

# 模块分层阶段一复审

- **对象**：`refactor/module-layering-a` 的 `3cede559`，基于 `dev` 的 `e58f528a`。
- **对照**：方案原文（`git show 729d5e3e:docs/specs/architecture/MODULE_LAYERING_ZH.md`）、
  [模块分层（当前）](../specs/architecture/MODULE_LAYERING_CURRENT_ZH.md)、
  [阶段一交付报告](MODULE_LAYERING_A_DELIVERY_ZH.md)。
- **目标**：Android、Web 用自己的 UI 调用下层库，Android 不带编辑器，Web 不带编辑器和时间轴。

## 结论

阶段一承诺的静态边界全部成立：层级脚本通过，include 写法统一，下层库对 `MiaCode` 没有任何链接级符号依赖，
Qt 私有头和第三方库都在允许的库内，QML 类型只注册一次，qrc 随所属库注册，除 `MiaCode` 外没有库依赖
`miacode_editor_core`。

「其他端用自己的 UI 调下层库就能跑起来」还没有达到。剩下的问题都在层级脚本看不到的地方：
音频后端、qrc 字符串、偏好写入、全局变量和素材目录。另有一项是本分支引入的回归（见 A2）。

14 个库的结论：`miacode_editor_core` 达标，其余 13 个基本达标、有遗留。

## 方法

| 检查 | 方式 |
| --- | --- |
| 层级 | `python scripts/governance/module_layering.py`：`Module layering is consistent.` |
| include 写法 | 全仓没有用 `<...>` 包含 `src/` 下的项目头，也没有 `../` 相对包含 |
| 链接级依赖 | 用 `nm` 读取 `build-msvc/Release/miacode_*.lib`，以每个库的未定义符号对照其他库的定义符号。取自 `3cede559` 提交前的最后一次 Release 构建 |
| qrc 字符串 | 扫描各库源码与 QML 中的 `:/`、`qrc:/` 字面量，对照资源所属的库 |
| 人工审查 | 每个库由一个只读审查代理检查，主审对较重的结论回到源码或二进制核实 |

本次没有构建，也没有运行 Spec。Android 与 WebAssembly 的判断都来自源码，没有真实工具链验证。
`web_module_boundary_spec` 和 `android_module_boundary_spec` 只证明这些库组合在 Windows 桌面上能链接、能运行。
符号表只反映非内联函数，头文件内联、模板和纯类型的使用要靠 include 与源码判断。

## 方案依据逐条核对

| 依据 | 状态 | 说明 |
| --- | --- | --- |
| 1 音符模型归 chart | 已解决 | `TimelineData.h`、`TimelineMarkerOffset.h` 在 `core/chart/model/`，解析器输出只依赖 chart 类型 |
| 2 timeline → muri 只来自 TimelineSlowRefresh | 部分 | `TimelineSlowRefresh` 已移入 analysis，`src/timeline` 没有 analysis 的 include 或符号。但 `timeline_quick` 直接使用 Muri（`TimelineQuickStateBridge.h:16`），基线就已存在，方案原判断不完整 |
| 3 偏好 IO | 部分 | 下层不再直接读写 `preferences.json`，但仍经端口读-改-写，export 另用 `QSettings`，见 B2 |
| 4 Qt 私有头 | 已解决 | 只在 stage_media，export 实际没有使用 |
| 5 timeline 间接依赖 BASS | 已解决 | `WaveformCache` 移入 audio，经 `AudioFileDecoder` 注入；`miacode_timeline.und` 没有 BASS 符号 |
| 6 场景数学依赖 QtMultimedia | 已解决 | scene 只剩注释提到 `QVideoFrame`。preview_quick 仍链接 QtMultimedia，见 B5 |
| 7 下层 qrc 打包在 app | 已解决 | 资源随库注册。但存在跨层的资源字符串引用，见 B1 |
| 8 LatencySandboxController 是 Session 的 friend | 阶段二 | `Session.h` 仍有 10 个 friend，与基线相同 |
| 9 preview QML 类型重复注册 | 已解决 | `src` 下没有 `qmlRegister*`，`main.cpp` 只导入一次插件 |
| 10 Spec 逐个编译源文件 | 部分 | 库源文件已改为链接。`MiaCode` 的源文件仍逐个列出，`PreferenceDocument.cpp` 被 8 个 Spec 各编译一次 |

## A. 阻断平台目标或本分支回归

### A1 Android 和 Web 没有可用的音频

- `third_party/bass/lib` 只有 `linux`、`macos`、`win64`、`winarm64`。`cmake/MiaCodeModules.cmake:284-301` 只处理
  WIN32、APPLE、Linux，Android 上 `miacode_audio_bass` 没有可链接的 BASS 库。
- Android 同时定义 `Q_OS_LINUX`，`src/audio/bass/BassPreviewAudioBackend_EngineInit.cpp` 的 PipeWire（约 :105）
  与 dlopen（约 :299）分支会被编入（推断），守卫应为 `Q_OS_LINUX && !Q_OS_ANDROID`。
- 仓库里没有非 BASS 的实现：`PreviewAudioBackend` 只有 BASS 后端，`AudioFileDecoder` 只有
  `bassAudioFileDecoder()`。未安装时 worker 进入 Degraded（`PreviewAudioWorker.cpp:282-287`），波形为空。
- `android_module_boundary_spec` 链接 `miacode_audio_bass`，只在 Windows 上运行，运行时依赖 `MiaCode` 的
  POST_BUILD 把 `bass.dll` 复制到共享输出目录（推断，未单独构建验证）。
- 当前规范把 `audio_bass` 列入 Android 组合，没有说明这一点；交付报告只提到 Web 的音频后端以后适配。

### A2 回归：`media_tools/net/` 被链进 `MiaCode.exe`

- 基线中 `src/tools/net/` 只由 `net_client_spec` 编译（`e58f528a:cmake/devtools/specs/core.cmake:399-412`）。
  现在它是 `miacode_media_tools` 的源文件，`src/app` 没有任何引用。
  拉入路径推断为 AUTOMOC 的合并单元：app 使用 `PvBatchCompressionWorker` 时，同一个 moc 目标文件会带入
  `NetBatch*Worker` 和 `NetClient`。没有用链接映射文件确认。
- 这违反「打包产物不变」。`core.cmake:277-280` 的注释和 `DEPENDENCY_ALLOWLIST.md` 关于网络代码不在产品中的说法也随之失效。

### A3 素材目录没有注入点（推断）

- `core/video/AssetPaths.h:12-31` 只从 `applicationDirPath()` 向上查找 `assets/`，另试 macOS bundle 路径，
  没有注入入口。`PreviewSfxAssets.h:180-215` 的 SFX 目录也依赖 `applicationDirPath()` 和环境变量
  `MAIMURI_PREVIEW_SFX_DIR`，后者没有登记在 `docs/ops/DEBUG_INDEX.md`。
- 使用方：`PreviewSceneAssetLoader.cpp`（描边、noteguide）、`TimelineNoteAssets.cpp:46-51`（时间轴默认皮肤，
  找不到 `tap.png` 时返回空素材集）。`assets/` 只在 Windows 的构建后步骤复制。
- 推断 Android 上这些素材都找不到：`applicationDirPath()` 指向 lib 目录，素材在 APK 内。没有在设备上验证。

## B. 违反方案约束的运行期耦合

### B1 qrc 字符串跨层引用

层级脚本不检查资源字符串。扫描结果：

| 位置 | 所属库 | 引用 | 资源所属 |
| --- | --- | --- | --- |
| `core/chart/IntroConfig.h:35-36,41` | chart | `qrc:/intro/qml/IntroOverlay.qml`、`qrc:/intro/templates/maimai_banner.json`、`:/intro/audio/track_start.wav` | export |
| `core/chart/IntroConfig.h:37-38` | chart | `qrc:/icons/app-original.png`、`:/icons/app-original.png` | MiaCode |
| `preview/stage_media/qml/PreviewSurface.qml:227` | stage_media | `qrc:/intro/qml/IntroOverlay.qml` | export |
| `preview/stage_media/qml/PreviewRuntimeView.qml:62` | stage_media | `qrc:/intro/qml/IntroOverlay.qml` | export |
| `src/intro/qml/*`（打包进 export 的 `intro.qrc`） | export | `qrc:/icons/app-original.png` | MiaCode |

`ChartAssetPaths.h` 用 `IntroConfig.h` 的 logo 作为背景回退，stage_media 因此也会回退到 app 的图标。
方案原文写「片头归 app」，实际片头配置在 chart、资源与 shader 在 export、合成 QML 的加载方在 stage_media。

### B2 偏好仍是读-改-写，不是「只接收值」

已定决策是「下层只接收值，持久化由 app 的设置宿主负责」。方案约束写的是「值或端口」，所以字面上不违规，
但当前实现与决策不一致，也没有登记为偏差。

- **scene**：`PreviewHudState.cpp:73-80` 读、`:443-470` 写 `app.video_export`，写入结果被忽略（`:452`、`:470`）。
  写入方只有 app（`PreviewSettingsModel.cpp:460`）。这个段同时被 export（`VideoExportPreferences.h:33-44`）和 app 读写，
  当前规范「库读写自己的 `app.<section>`」不成立。
- **缓存**：`PreviewHudState.cpp:103-106` 首次读取后置 `initialized = true`，提供者安装晚于首次读取时，空值会一直保留。
- **export**：`CoverCompositionState.cpp:145-297` 读写 `app.cover_export`，`VideoExportPreferences.h:33-44` 读写 `app.video_export`。
- **export 绕过端口**：`VideoExportEncoder.cpp:79-100` 用 `QSettings` 写 `MiaCode/VideoExportRuntime.ini`，
  保存硬件编码器探测结果。基线已存在，是下层库中唯一的 `QSettings`。
- **chart**：`transform/ChartNormalization.cpp` 的 `chartNormalizationOptionsFromPreferences` 与
  `saveChartNormalizationOptionsToPreferences` 解析 `preview` 段的键，只有 app 调用。
- 没有提供者时 `FontLibrary.cpp:14-17` 的目录为空，推断会落到当前工作目录。

### B3 翻译目录只在 app

`CMakeLists.txt:791-806` 以 `-idbased` 生成 `.qm`，只注册到 `MiaCode` 的 `miacode_i18n` 资源。下层库使用 ID 形式的
`qtTrId`：`core/analysis/MuriTypes.cpp:386-394`、`timeline/quick/TimelineQuickStateBridge.cpp:62-66`、
`media_tools`（62 处）。其他端不带 app 的翻译，界面会显示 `validation.muri.kind.overlap` 这类 ID。

### B4 隐式全局传值

- **时间轴配色**：`timeline/TimelineThemeConfig.h:129-142` 是进程级静态变量，默认无效颜色。唯一写入方是 app 的
  `TimelineThemeBridge.cpp:57`，scene 构建在 `TimelineSceneStateBuilder.cpp:522` 隐式读取。`MiaCode.Timeline` 模块没有
  设置配色的入口，Android 自带 UI 时颜色未定义。
- **片头音量**：`core/scene/PreviewSfxAssets.h:30-59` 的 `selectedIntroSound*` 是函数内静态变量，由 app 写入
  （`EditorDisplay.cpp`、`ExportSession.cpp`），audio 读取（`PreviewAudioSettings.h:126`）。

### B5 preview_quick 为死代码链接 QtMultimedia

`cmake/MiaCodeModules.cmake:425,428` 让 preview_quick PUBLIC 链接 `Qt6::Multimedia` 并定义 `HAVE_QT_MULTIMEDIA=1`，
唯一用途是 `PreviewRuntime::setVideoFrame`、`setResolvedStageVideoFrame`（`PreviewRuntime.cpp:13-14,551-582`）。
全仓没有调用方，基线也没有。视频帧实际经 stage_media 送到 QML 的 `VideoOutput`。去掉后 Web 组合不再需要 QtMultimedia。
`module_layering.py` 把 preview_quick 列在 QtMultimedia 允许名单里，回归时不会报错。

### B6 预览合成 QML 放在 stage_media

`PreviewSurface.qml`、`PreviewRuntimeView.qml` 组合的是 preview_quick 的 `PreviewQuickSceneRoot`、`PreviewQuickHudLayer`
和片头层，只有 `PreviewStageMediaItem.qml` 真正属于 stage_media。不带 stage_media 的宿主拿不到现成的合成界面。
`PreviewRuntimeView.qml` 除 `resources/preview_runtime_qml.qrc` 外没有引用。stage_media 对 preview_quick 没有 C++
include 和链接级符号，这条边只来自这两个 QML。

## C. 应改进（不阻断平台目标）

- **base**
  - `CrashRecovery` 只有 app 使用，并硬编码 app 的自动保存目录和 `maidata.txt`（`CrashRecovery.cpp:397-401`）。
  - `GpuDevicePolicy` 是 DXGI 与进程角色逻辑，使用方是 app 和 export，迫使 base 在 Windows 链接 dxgi。
  - `LogEmissionPolicy.h` 的若干日志闸门只服务 stage_media 和 timeline_quick。
  - `DebugOptions.h:10-14` 在公共头里包含 `windows.h`。
- **chart**
  - `latencyMeterIdForTimingMetadata`（`SimaiTimingMetadata.h:39`）是延迟检测概念，只有 app 调用。
  - 17.9MB 的 `slide_data.json` 被解析三次（`SimaiParser.cpp` 两处、`MuriSlideReferenceData.cpp` 一处），打开失败时静默返回空。
  - `g_allowNegativeHs` 是非原子全局开关，由 app 注入。
- **analysis**：`TimelineSlowRefresh` 头里混有只依赖 chart 的预览刷新部分，`buildTimelinePreviewRefreshResult` 没有调用方；`MuriDiagnostic.title` 硬编码中文。
- **editor_core**：补全状态机、选区事务构造、行内词法仍在 app/ui（`EditorController.cpp`、`ScintillaDslStyler.cpp`）。`core/chart/transform`、`selection` 约 5900 行只有 app 使用。
- **scene**
  - `makeMarkerAnalysisKey` 只依赖 `TimelineNoteMarker`，可以下沉到 chart。
  - `PreviewVideoFrameHandle` 的负载没有消费者，却每帧分配。
  - 两个内嵌字体共约 24MB，对 Web 包体有影响。
- **audio**
  - 公共端口类型仍叫 `BassEmergencyPauseResult`（`PreviewAudioWorkerFactory.h:14`）。
  - `PreviewAudioOutputGlitchProbe.h`、`PreviewAudioOutputGlitchRing.h` 只有 audio_bass 使用。
  - `PreviewAudioBackend.h` 为了 `TimelineNoteMarker` 包含 `PreviewSfxTimeline.h`。
  - 提供者必须在创建 worker 前安装（`PreviewAudioWorker.h:54`），文档没有写明。
  - worker 无条件起 `std::thread`，单线程 WASM 不可用（推断）。
- **audio_bass**
  - BASS include 目录与库以 PUBLIC 暴露，`BassFlacPlugin.h` 在公共头里包含 `bass.h`。
  - `disableBassDefaultDeviceEntry` 只在 `main.cpp` 调用，其他宿主若先离线解码可能重现 err=37。
  - `avrt` 以 PUBLIC 链接但未使用。
- **timeline**
  - 模型层以 `QPixmap` 承载音符素材（`TimelineNoteAssets.h:12-21`），需要 GUI 线程。
  - `TimelineQuickModelParser.cpp`（1213 行）自带一份 Simai 解析和时间公式，与 chart 重复。
  - CMake 声明 timeline PUBLIC 依赖 analysis，源码没有使用。
- **timeline_quick**
  - `if (WIN32)` 中 PRIVATE 链接的 d3d11、dxgi 没有被使用。
  - `TimelineQuickItem.cpp:26` 包含未使用的 `PreviewInteractionConfig.h`。
  - `TimelineQuickStateBridge` 不是 QML 类型，只能由 C++ 创建。
- **stage_media**：配置期并不可选，`CMakeLists.txt` 无条件 REQUIRED `MultimediaQuickPrivate`，QtAVPlayer/FFmpeg 块没有平台守卫。
- **export**
  - `CoverComposer.qml` 需要 `MiaCode.Preview`，但 export 不链接插件，依赖宿主导入。
  - 导出音频写死 `BassExportAudioBackend`（`VideoExportPipeline.cpp:173-175`），没有注入点。
  - `QOpenGLFramebufferObject` 所需的 Qt6::OpenGL 只经传递获得。
- **media_tools**
  - ffmpeg 路径按 app 的部署和仓库布局查找（`PvBatchCompressionWorker.cpp:240-275`），同类逻辑在 app、export 各有一份。
  - chart 依赖只来自 `ChartAssetPaths.h` 的 4 个内联函数，该头又带入 `IntroConfig.h`。
- **app**
  - `runtime/shell/ShellHost.cpp` 按名字读取 QML 属性。
  - `QuickShellPreviewCompositeSurface.cpp` 用 `setProperty("runtime"/"mediaHost")` 绑定 stage_media 的 QML。
  - 延迟检测 DSP（`LatencyAnalysis.cpp`）、`ContentDurationConfig.h`、皮肤目录发现、`ExportSnapshot.cpp` 的导出规则仍在 app。
  - `normalizeLanguageToken` 有 4 份，ffmpeg 路径解析有 3 份。

## 按库结论

| 库 | 结论 | 主要遗留 |
| --- | --- | --- |
| `miacode_base` | 基本达标 | 上层专用内容（崩溃恢复、GPU 策略、日志闸门）；公共头包含 `windows.h` |
| `miacode_chart` | 基本达标 | `IntroConfig` 跨层资源引用；偏好键读写；`slide_data.json` 重复解析 |
| `miacode_analysis` | 基本达标 | Muri 文案依赖 app 翻译 |
| `miacode_editor_core` | 达标 | 编辑逻辑仍在 app/ui，只影响桌面 |
| `miacode_scene` | 基本达标 | 偏好读-改-写与缓存；素材目录无注入点；片头音量全局变量 |
| `miacode_audio` | 基本达标 | 没有非 BASS 后端与解码器；`Bass` 命名残留 |
| `miacode_audio_bass` | 基本达标 | 没有 Android 分支与库；BASS 头 PUBLIC 暴露 |
| `miacode_timeline` | 基本达标 | 全局配色；`QPixmap`；重复的 Simai 解析 |
| `miacode_timeline_quick` | 基本达标 | 配色入口在 app；多余的 d3d11/dxgi |
| `miacode_preview_quick` | 基本达标 | 为死代码链接 QtMultimedia；素材目录无注入点 |
| `miacode_stage_media` | 基本达标 | 合成 QML 放错层并加载 export 的片头 |
| `miacode_export` | 基本达标 | `QSettings` 直写；偏好读-改-写；片头引用 app 图标；导出音频写死 BASS |
| `miacode_media_tools` | 基本达标 | `net/` 进入产品（回归）；ffmpeg 路径耦合部署布局 |
| `MiaCode` | 基本达标 | 按名字读 QML 属性；领域代码滞留 |

## 文档与代码不一致

- 交付报告「export 通过依赖 stage_media 使用 `PreviewSharedD3D11Device`」不成立：export 既不包含也不链接 stage_media，
  该设备只有 app 使用，export 自己创建 D3D11 设备。
- 当前规范：
  - Qt 模块一节没有记录 preview_quick 链接 QtMultimedia。
  - Qt 私有头写「stage_media 和 export」，export 实际没有使用；`module_layering.py` 对 export 放行 QtAVPlayer 与私有头，比实际宽。
  - Android 组合包含 `audio_bass`，没有说明缺少 Android 版 BASS。
  - 注入点表写「库读写自己的 `app.<section>`」，与实际和决策都不一致；也没有列出 timeline 的注入点（配色、皮肤目录、字体）。
  - 「不属于任何库」的目录漏了 `src/wrapper`。
  - timeline 的允许依赖写 analysis、audio，实际 analysis 只有 timeline_quick 使用。
- 交付报告的偏差表没有登记偏好端口与「只接收值」的差异，也没有登记 `intro.qrc` 归 export。
- `DEPENDENCY_ALLOWLIST.md`：
  - bass 写成「全平台」。
  - d3d11 行仍写 `src/preview/runtime/`。
  - 网络与 `Qt6::Network` 的说法已过时。
  - soundtouch 的使用方写的是 `src/audio` 与 media_tools，源码里已没有引用。
- 过时注释：`TimelineMarkerOffset.h` 仍说位于 `src/timeline/`；多处仍引用已删除的 `MainWindow`。

## 层级脚本可补的检查

- qrc 字符串的归属，即本次 B1 使用的扫描。
- base 到 scene 禁用 QtQuick、QtQml，并核对 CMake 的 Qt 链接。
- 收紧放行名单：export 对 QtAVPlayer 与 Qt 私有头的放行、preview_quick 与 audio_bass 对 QtMultimedia 的放行，现在都用不到。
- app 内部方向：services 不包含 runtime；platform、quick_shell 不包含 ui、runtime。

## 阶段二相关观察

- `Session.h` 有 10 个 friend，`SessionMembers.inc` 795 行；`LatencySandboxController.cpp` 约 70 处直接访问 Session 私有成员。
- `ChartWorkspace`、`ChartWorkspaceFileService`、`AnalysisService` 只依赖 chart、analysis、base，可以直接上提；打开后的编排在 `ui/document/DocumentModel.cpp`。
- `PlaybackCoordinator` 的 4 个端口是干净的，但构造依赖 `RuntimeContext`，其中内嵌 `TimelineQuickModel`，与 Web 不带时间轴冲突。
- 播放链路硬连 BASS 解码（`TimelineFlow.cpp`、`Shared.cpp`），延迟检测绕过 `AudioFileDecoder` 直接调用离线解码。

## 需要决定

1. **Android 与 Web 的音频后端**：补 Android 版 BASS 和 CMake 分支，还是提供一个 QtMultimedia 后端与解码器供两端共用。
2. **偏好**：保留端口读-改-写并登记为偏差，还是按原决策把持久化移到 app 设置宿主、下层只接收值。
3. **片头归属**：`IntroConfig`、`intro.qrc`、片头 QML 归 app 还是 export，以及 stage_media 合成 QML 如何取得片头层。
4. **`net/`**：退回 Spec 专用，还是拆成独立库并避免进入 `MiaCode.exe`。
