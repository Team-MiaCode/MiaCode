---
lifecycle: stable-current
owner: src
canonical_id: architecture.module-layering
last_verified: 2026-10-06
code_anchors: ["cmake/MiaCodeModules.cmake", "cmake/MiaCodeModuleHelpers.cmake", "scripts/governance/module_layering.py", "src/common/PreferenceProvider.h", "src/audio/PreviewAudioWorkerFactory.h", "src/audio/AudioFileDecoder.h", "src/core/scene/PreviewFrameState.h", "src/app/main.cpp", "src/tools/boundary"]
---

# 模块分层（当前）

`src/` 下的产品代码按目录归属到静态库，`MiaCode` 可执行文件链接这些库。每个目录只属于一个库，
下层库不依赖 `MiaCode`。后续拆分方向见 [模块分层与解耦方向](MODULE_LAYERING_ZH.md)；
`MiaCode` 内部的所有权见 [当前应用架构](../ui/CURRENT_ARCHITECTURE_ZH.md)。

## 库与目录

库定义在 `cmake/MiaCodeModules.cmake`（`miacode_add_module`），源文件逐个列出。

| CMake 目标 | 源目录 | 主要内容 | 允许依赖 |
| --- | --- | --- | --- |
| `miacode_base` | `src/common` | 日志、调试选项、诊断、任务取消、文件戳、本地化文本、崩溃恢复、看门狗、偏好端口 `PreferenceProvider` | Qt |
| `miacode_chart` | `src/core/chart` | 文档、解析、变换、选择；音符模型 `model/TimelineData.h`（含 `MuriPadTimeEntry`）与 `model/TimelineMarkerOffset.h`；`ChartClockCount`、`ChartAssetPaths`、`IntroConfig`；`slide_data` 资源及其共享解析入口 `SlideReferenceData` | base |
| `miacode_analysis` | `src/core/analysis` | Muri 产品代码与报告类型 `MuriTypes`、`MuriConfig`、`MuriRenderOptions`；分析流水线 `TimelineSlowRefresh` | chart |
| `miacode_editor_core` | `src/editor` | 文本策略、补全、书签语法 | chart |
| `miacode_scene` | `src/core/scene`、`src/core/video` | 场景数学、Preview*Config、SFX 时间线语义、`AssetPaths`；字体资源 | analysis |
| `miacode_audio` | `src/audio` | 音频后端接口与提供者、worker、协议、设置、`QtPreviewSfxRuntime`、`WaveformCache` 与解码器接口 `AudioFileDecoder` | scene |
| `miacode_audio_bass` | `src/audio/bass` | BASS 后端、设备租约、紧急暂停、离线解码 | audio |
| `miacode_timeline` | `src/timeline` | 时间轴模型 | analysis、audio |
| `miacode_timeline_quick` | `src/timeline/quick` | QSG 图层；QML 模块 `MiaCode.Timeline 1.0` | timeline |
| `miacode_preview_quick` | `src/preview/quick_scene`、`src/preview/runtime` | QSG 场景、`PreviewRuntime`、素材与纹理仓库；shader 与判定特效资源；QML 模块 `MiaCode.Preview 1.0` | scene |
| `miacode_stage_media` | `src/preview/stage_media` | `PreviewStageMediaHost`、`PreviewSharedD3D11Device`、`PvMemoryDiagnostics`、QtAVPlayer；`qrc:/preview/runtime/qml` | scene、preview_quick |
| `miacode_export` | `src/export` | 视频导出（`video_export`）、封面导出（`cover_export`）、QSG/D3D11 导出会话（`session`）、导出音频；片头资源与 shader | 除 `MiaCode` 外任意库 |
| `miacode_media_tools` | `src/media_tools` | PV 压缩（`media`）、ZIP 打包（`zip_export`）、网络客户端（`net`） | chart |
| `MiaCode` | `src/app` | UI、services、runtime（含延迟检测 `runtime/latency`）、quick_shell、platform、进程入口 | 任意库 |

`src/tools` 只放 Spec，`src/devtools` 是命令行诊断入口，`src/intro` 是片头素材，`src/extensions` 只由
扩展清单 Spec 编译；这些目录不属于任何库。依赖可传递：允许依赖的库所依赖的库同样可用。

## 约束

- **include**：项目头文件一律写成以 `src` 为根的完整路径。每个库 PUBLIC 导出 `src` 根；唯一的例外是
  qmltyperegistrar 生成的注册代码只按文件名包含 `QML_ELEMENT` 头文件，所以 `miacode_preview_quick`、
  `miacode_timeline_quick` 和 `MiaCode` 以 PRIVATE 方式加入这些头文件所在目录，项目代码不依赖它。
- **Qt 模块**：base 到 scene 只用 QtCore/QtGui；audio 加 QtMultimedia；QSG 库加 QtQuick/QtQml。
  Qt 私有头只出现在 `stage_media`（QtAVPlayer 与 `MultimediaQuickPrivate`）和 `export`。场景状态不含
  `QVideoFrame`：视频帧以 `PreviewVideoFrameHandle`（共享的不透明句柄）传递。
- **第三方**：BASS 只在 `audio_bass` 和 `export`；QtAVPlayer/FFmpeg 只在 `stage_media`；ScintillaQuick 只在
  `MiaCode`；miniz 与 QtNetwork 只在 `media_tools` 和 `MiaCode`。外部链接项登记在
  [依赖 allowlist](../../ops/DEPENDENCY_ALLOWLIST.md)。

## 注入点

| 端口 | 定义 | 安装方 |
| --- | --- | --- |
| 偏好 | `common/PreferenceProvider.h`：库读写自己的 `app.<section>`，并取得偏好目录与界面语言 | `app/runtime/settings/PreferenceDocumentProvider`；`main()` 的 GUI 路径、`runCliVideoExport`、`runCliVideoExportWorker` 各自安装 |
| 预览音频后端 | `audio/PreviewAudioWorkerFactory.h` 的 `PreviewAudioBackendProvider`（后端工厂与紧急暂停） | `main()` 安装 `bassPreviewAudioBackendProvider()` |
| 波形解码 | `audio/AudioFileDecoder.h`，`WaveformCacheService::setDecoder` | 播放协调器创建波形缓存时注入 `bassAudioFileDecoder()` |
| 导出页 | `app/services/ExportPagePort.h` | `Bootstrap` 经 `ApplicationServices::setExportPageFactory` 提供 `ExportSession` |

未安装偏好提供者时读取为空、写入失败；未安装音频后端时 worker 报告工厂返回空；未注入解码器时波形没有采样。

## 资源与 QML 模块

资源随所属库注册，路径保持不变。`.qrc` 经 `miacode_add_qrc` 转成按目标注册的 `qt_add_resources`，
静态库的资源初始化对象由 Qt 链进使用它的可执行文件，不需要 `Q_INIT_RESOURCE`。

| 资源 | 所属库 | 路径 |
| --- | --- | --- |
| `resources/slide_data.qrc` | chart | `:/data/slide_data.json` |
| `resources/fonts.qrc` | scene | `:/fonts/*.ttf` |
| `resources/preview_judge_effects.qrc`、`preview_sprite_shaders` | preview_quick | `:/preview/judge_effects/*`、`:/src/preview/quick_scene/shaders/*.qsb` |
| `resources/preview_runtime_qml.qrc` | stage_media | `qrc:/preview/runtime/qml/*` |
| `resources/intro.qrc`、`intro_shaders` | export | `qrc:/intro/*`、`:/src/intro/shaders/*.qsb` |
| `resources/app_icons.qrc`、`ui_shaders`、`miacode_i18n` | MiaCode | `:/icons/*`、`:/config/shortcuts.json`、`:/src/app/ui/shaders/*.qsb`、`:/i18n/*` |

`MiaCode.Preview` 与 `MiaCode.Timeline` 由各自库的 `qt_add_qml_module` 提供静态插件，`main.cpp` 用
`Q_IMPORT_QML_PLUGIN` 导入一次，进程内所有 QML engine（主界面、预览合成面、封面合成、导出）共用；
不再有分散的 `qmlRegisterType`。`MiaCode.UI` 仍是 `MiaCode` 自身的模块。

## app 内部方向

`app/services`、`app/runtime` 不包含 `app/ui`；只有进程入口包含 `app/MainEntrypoints.h`，
GPU 与进程诊断声明在 `app/platform/PlatformDiagnostics.h`。

## 检查

- `scripts/governance/module_layering.py`：目录归属、include 方向、第三方与 Qt 私有头、`miacode_*` 链接边、
  库源文件清单；`module_layering_test.py` 覆盖其行为。CI 的 `Module layering` 步骤运行两者。
- `web_module_boundary_spec` 只链接 base、chart、analysis、scene、audio、preview_quick；
  `android_module_boundary_spec` 再加 timeline、timeline_quick、audio_bass。两者在不链接 `MiaCode` 的情况下
  运行各库的代表路径，并确认静态库资源和 QML 模块可用。
- Spec 与命令行诊断链接库，只直接列出 `MiaCode` 的源文件（可执行文件无法被链接）。
