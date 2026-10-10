# MiaCode 依赖 allowlist

> 归属：[当前应用架构](../specs/ui/CURRENT_ARCHITECTURE_ZH.md)。
>
> 本文登记 **`MiaCode` 主程序及其链接的 `miacode_*` 库链接的每一个外部库**：属于哪一层、
> 在什么平台条件下存在、代码里的直接使用点在哪、什么时候加载、怎么验证。库的划分见
> [模块分层](../specs/architecture/MODULE_LAYERING_CURRENT_ZH.md)；`miacode_*` 库之间的边由
> `scripts/governance/module_layering.py` 检查，不在本文登记。目标不是把部署包里的 DLL 数量压到最低，
> 而是让每一条依赖都能说明它为谁存在——没有主人的依赖不许留在链接行上。
>
> **漂移守卫**：`dependency_allowlist_spec`（`src/tools/deps/DependencyAllowlistSpec.cpp`）
> 解析 `CMakeLists.txt` 与 `cmake/MiaCodeModules.cmake` 里 `MiaCode` 和 `miacode_*` 的全部
> `target_link_libraries(…)`、`miacode_add_module(… PUBLIC/PRIVATE …)` 链接项，并与下面三张表比对。
> 新增依赖没写进表、表里留着已删依赖、禁止表里的库被链接、Qt 版本没锁、QtAVPlayer 头文件
> 泄漏出媒体适配层——五种漂移都会让 `ctest -R dependency_allowlist_spec` 失败。
> **改依赖和改本文必须同一次提交。**

Qt 最低版本锁定：`6.10`

版本锁不是风格问题：`Qt6::MultimediaQuickPrivate` 是私有模块，没有跨版本兼容承诺，
所以 `CMakeLists.txt` 里**每一处** `find_package(Qt6 <ver> …)` 都必须写同一个版本号，
守卫会逐个比对。

## 分层

| 层 | 含义 |
| --- | --- |
| 宿主 | Qt Quick/QML 应用宿主本身：没有它进程起不来 |
| 渲染 | QSG 场景、着色器、矢量图标 |
| 媒体 | 音频后端与背景视频解码 |
| 导出 | 视频/封面/ZIP 导出管线 |
| 平台 | 只在某个操作系统上存在的系统库 |
| 遗留 | 已判定要移除、但当前仍被链接（各自注明退出阶段） |

## 允许链接进 `MiaCode`

| 依赖 | 分层 | 平台条件 | 直接使用点 | 加载时机 | 验证方式 |
| --- | --- | --- | --- | --- | --- |
| `Qt6::Core` | 宿主 | 全平台 | 全模块 | 进程启动 | 链接期；全量 CTest |
| `Qt6::Concurrent` | 宿主 | 全平台 | `src/media_tools/net/NetResourceOperation` 与 `NetUploadOperation`：下载发布、素材摘要与上传快照的后台计算 | Net 资源发布或上传素材处理 | 链接期；`net_download_flow_spec`；上传源码与编译核对 |
| `Qt6::Gui` | 宿主 | 全平台 | `QGuiApplication`、`QImage`/`QPainter`（封面与 HUD 合成）、字体 | 进程启动 | 链接期；`cover_composite_renderer_spec` |
| `Qt6::Qml` | 宿主 | 全平台 | `QQmlApplicationEngine`（`Bootstrap`）、全部 `Qml*` 模型 | 进程启动 | 链接期；`qml_*_spec` 组 |
| `Qt6::Quick` | 宿主 | 全平台 | `QQuickWindow`、`src/preview/quick_scene/`（`miacode_preview_quick`）、`src/timeline/quick/`（`miacode_timeline_quick`）、导出会话（`miacode_export`） | 进程启动 | 链接期；`qml_*_spec` 组 |
| `Qt6::QuickControls2` | 宿主 | 全平台 | `src/app/ui/` 全部 QML 页面与控件 | 首个 QML 组件实例化 | 链接期；`qml_main_menu_spec` 等 |
| `ScintillaQuick::ScintillaQuick` | 宿主 | 全平台 | `src/app/ui/editor/` 的谱面源码编辑器（`ScintillaEditorBridge`、`ScintillaDocumentAdapter`、`ScintillaDslStyler`）；子模块 `third_party/ScintillaQuick`，只链接进 `MiaCode` | 编辑器页面首次创建 | 链接期；`qml_*_spec` 组；编辑器手工回归 |
| `Qt6::Quick3D` | 渲染 | 全平台 | `src/app/ui/pet/PetOverlay.qml` 的 `View3D`、`PerspectiveCamera`；`src/app/ui/pet/model/Fox.qml` 的 `Node`、`Model`、`DefaultMaterial`（桌宠狐狸） | 桌宠窗口首次显示（`PetOverlayController` 置 `visible`） | 链接期；桌宠手工回归 |
| `Qt6::Quick3DHelpers` | 渲染 | 全平台 | `src/app/ui/pet/model/Fox.qml` 的 `ProceduralMesh`：狐狸的盒式几何体在运行时生成（8 处），不走预烘的 `.mesh` 资源 | 同 `Qt6::Quick3D` | 链接期；桌宠手工回归 |
| `Qt6::Multimedia` | 媒体 | 全平台 | `PreviewAudioDeviceWatcher` 的设备枚举（`miacode_audio`）、`PreviewRuntime` 把 `QVideoFrame` 包成场景的不透明句柄（`miacode_preview_quick`）、`PreviewStageMediaHost` 的视频播放与 `QVideoFrame` 桥接（`miacode_stage_media`） | 音频设备扫描 / 视频首帧解码 | 链接期；`HAVE_QT_MULTIMEDIA=1`；预览手工回归 |
| `Qt6::MultimediaQuickPrivate` | 媒体 | `WIN32 OR APPLE OR Linux` | **不由 `src/` 直接使用**；仅供 `third_party/QtAVPlayer` 的 `QT_AVPLAYER_MULTIMEDIA` 桥编译 `QAVVideoFrame -> QVideoFrame` | 背景视频首帧解码 | 链接期；`qtavplayer_platform_spec`；本文「QtAVPlayer 媒体适配层」表 |
| `Qt6::Network` | 宿主 | 全平台 | `NetworkUpdateFetcher` 获取更新 manifest；`src/media_tools/net/NetHttpTransport` 承接 Majdata 查询、下载、账户与上传请求 | 更新检查或 Net 任务启动 | 链接期；`update_service_spec`、`net_http_transport_spec`、`net_provider_spec`、`net_download_flow_spec`；TLS 后端插件属打包验收 |
| `${QtAVPlayer_LIBS}` | 媒体 | `WIN32 OR APPLE`（需 `MIACODE_FFMPEG_DEV_DIR`）；Linux 用主机 pkg-config FFmpeg + libva | `PreviewStageMediaHost*`（PV/BG 解码）、`PreviewSharedD3D11Device`（D3D11VA 共享设备） | 背景视频首帧解码 | `qtavplayer_platform_spec`；macOS 打包契约 |
| `PkgConfig::MIACODE_FFMPEG` | 媒体 | `Linux`（Win/macOS 改走 `MIACODE_FFMPEG_DEV_DIR` 的项目 SDK，见上一行） | QtAVPlayer 的解码依赖，由主机 pkg-config 提供：`libavfilter`、`libavcodec`、`libavformat`、`libavutil`、`libswresample`、`libswscale` | 背景视频首帧解码 | 链接期（Linux 构建）；`qtavplayer_platform_spec` |
| `PkgConfig::MIACODE_VAAPI` | 媒体 | `Linux` | QtAVPlayer 在 Linux 的 VAAPI 硬件解码路径：`libva`、`libva-drm`、`libdrm` | 背景视频首帧解码（硬件解码可用时） | 链接期（Linux 构建）；`qtavplayer_platform_spec` |
| `soundtouch` | 媒体 | 全平台 | 变速播放与音频处理（`src/audio/`、`src/media_tools/media/`） | 首次变速播放 / 音频处理作业 | 链接期；音频手工回归 |
| `bass` | 媒体 | 全平台（Win: `bass.lib`，macOS: `libbass.dylib`，Linux: `libbass.so`） | `BassPreviewAudioBackend`、`BassExportAudioBackend`、`OfflineAudioDecoder` | 预览、导出和离线解码初始化 | 链接期；macOS 打包契约校验 dylib 已随包 |
| `bassmix` | 媒体 | 全平台 | `BassPreviewAudioBackend`、`BassExportAudioBackend` 的混音总线 | 预览或导出混音初始化 | 链接期；打包契约校验原生库已随包 |
| `miniz` | 导出 | 全平台 | `ChartZipPackager` 打包导出；`NetClient::packNetChartFolderZip` 打包下载素材 | ZIP 导出或 Net 下载发布 | `chart_zip_packager_spec`、`net_download_flow_spec` |
| `-framework AppKit` | 平台 | `APPLE` | `NativeWindowThemeMac.mm`、`WindowChrome.mm`（原生标题栏/外观） | 根窗口创建 | 链接期；macOS 冷启动走查 |
| `-framework QuartzCore` | 平台 | `APPLE` | `WindowChrome.mm` 的 `CATransaction` 管理原生窗口材质图层更新 | 窗口材质区域更新 | 链接期；`dependency_allowlist_spec` |
| `d3d11` | 平台 | `WIN32` | 共享预览设备、D3D11 导出会话、stage-media host（`src/preview/runtime/`） | 预览首次创建渲染设备 | 链接期；Windows 冷启动走查 |
| `dxgi` | 平台 | `WIN32` | `GpuDevicePolicy`、`ProcessDiagnostics`、`TimelineQuickItem`（适配器枚举与显存计量） | 启动诊断 / 预览创建 | 链接期；Windows 冷启动走查 |
| `d3dcompiler` | 平台 | `WIN32`（MinGW 必需；MSVC 走 `#pragma comment`） | QtAVPlayer 的 `D3DCompile` | 背景视频首帧解码 | 链接期（MinGW 缺失即链接失败） |
| `opengl32` | 平台 | `WIN32`（MinGW） | QtAVPlayer D3D11/OpenGL 纹理互操作 | 背景视频首帧解码 | 链接期 |
| `winmm` | 平台 | `WIN32` | 高精度多媒体计时器 | 播放/导出计时启动 | 链接期 |
| `ole32` | 平台 | `WIN32` | Windows Core Audio 端点通知 COM API | 音频设备枚举 | 链接期 |
| `avrt` | 平台 | `WIN32` | `AvSetMmThreadCharacteristicsW`（多媒体线程优先级） | 音频/渲染线程启动 | 链接期 |
| `User32` | 平台 | `WIN32` | `RegisterPowerSettingNotification` | 启动诊断注册 | 链接期 |
| `Advapi32` | 平台 | `WIN32` | `src/app/services/net/NetAccountStore` 使用 Windows Credential Manager 存储账户凭据 | 账户凭据读取、保存和删除 | 链接期；凭据存储源码核对；账户验收由用户执行 |
| `Wtsapi32` | 平台 | `WIN32` | 会话锁定/解锁通知（卡顿冻结诊断） | 启动诊断注册 | 链接期 |
| `version` | 平台 | `WIN32`（MinGW） | 启动诊断的文件版本查询 | 启动诊断 | 链接期 |
| `rstrtmgr` | 平台 | `WIN32`（MinGW） | 媒体工具的 Restart Manager 占用进程查找 | 媒体工具报「文件被占用」时 | 链接期 |
| `dwmapi` | 平台 | `WIN32` | `WindowChrome` 的 `DwmExtendFrameIntoClientArea` | 根窗口创建 | 链接期；Windows 冷启动走查 |
| `${CMAKE_DL_LIBS}` | 平台 | `Linux`（只在 Linux BASS 块里加入链接行；macOS 的 `dl` 由 libSystem 隐式提供，无需显式链接） | `src/audio/bass/BassPreviewAudioBackend_EngineInit.cpp` 用 `dlopen`/`dlsym` 载入 `libbass_fx.so` 并取 `BASS_FX_TempoCreate` | 首次变速播放（BASS FX 引擎初始化） | 链接期（Linux 构建）；音频手工回归 |

## 运行时插件（不在链接行）

下面的库随打包产物发布、在运行时由已链接的库加载，不出现在任何 `target_link_libraries` 里。

| 依赖 | 分层 | 平台条件 | 加载方 | 加载时机 | 验证方式 |
| --- | --- | --- | --- | --- | --- |
| `bassflac` | 媒体 | 全平台 | `BASS_PluginLoad`（`src/audio/bass/BassFlacPlugin.h`）：预览、导出和离线解码中的 FLAC 格式支持 | 音频后端初始化或离线解码时加载 | 运行时插件加载；打包契约校验原生库已随包 |

## 构建期组件

`find_package(Qt6 … REQUIRED COMPONENTS …)` 里可以出现不链接进运行时的组件，但必须在这里
说明——否则一个「REQUIRED 但没人链接」的组件只会让别人的机器白白配置失败。守卫要求产品作用域
`find_package` 的每个组件要么在允许表里有对应的 `Qt6::<组件>`，要么出现在下表。

| 组件 | 用途 | 为什么不算运行时依赖 |
| --- | --- | --- |
| `LinguistTools` | `qt_add_lrelease` 把 `translations/*.ts` 编译成 `.qm`，产物再作为资源打进产品 | 只在构建期运行 `lrelease`；产物是资源文件，`MiaCode` 不链接 `Qt6::LinguistTools` |
| `ShaderTools` | `qt6_add_shaders` 把 `src/intro/shaders/*.frag`、`*.vert` 编译成 `.qsb` 资源 | 只在构建期运行 `qsb` 工具；产物是资源文件，`MiaCode` 不链接 `Qt6::ShaderTools` |

> 2026-09-01 本次同时删掉了 `OpenGL` 组件：它被写成 `REQUIRED` 但没有任何 target 链接
> `Qt6::OpenGL`。运行时的 `QtOpenGL` 框架是 `Qt6::Quick` 的传递依赖，与这个组件声明无关。

## 禁止链接进 `MiaCode`

| 依赖 | 原因 | 归属 |
| --- | --- | --- |
| `Qt6::Test` | 只属于 dev-tools spec 可执行文件，不得进入产品进程。 | `MIACODE_BUILD_DEV_TOOLS` 分支下的各 spec target |
| `Qt6::Svg` | 产品齿轮图标已迁移到 `src/app/ui/resources/icons/settings.svg`；产品 C++ target 不得直接编译或链接 Qt SVG。 | QML 资源的运行时 SVG plugin 是否随打包产物提供，另行按打包验收。 |
| `Qt6::Widgets` | 产品页面、文档保存/离开确认和运行时辅助件已迁移到 QML/纯 Qt API；产品 target 不得重新引入 QWidget 生命周期。 | 仅允许遗留 dev-tool spec target 使用；重新进入 `MiaCode` 链接行视为回退。 |

## 传递依赖：本文管不到、也不假装管得到的部分

上面三张表管的是**直接链接边**。可执行文件实际加载的框架比它多，因为 Qt 模块之间自己有依赖。
把这一点写清楚，是为了避免「从链接行删掉 X」被误读成「部署包里没有 X 了」：

- **`QtNetwork` 同时存在直接与传递依赖**：更新检查和 Net 业务通过链接项使用它，
  Qt QML、Quick 和 Multimedia 也会带入该模块。依赖清单登记产品的直接使用点，部署包由 Qt 的依赖图决定。
- **`QtOpenGL` 同理**，由 `Qt6::Quick` 带入。
- 复核方法（macOS）：

```bash
otool -L build-macos/MiaCode.app/Contents/MacOS/MiaCode | grep Qt
```

  Windows 用 `dumpbin /dependents`。守卫不跑这一步：它依赖已构建产物和平台工具链，属于
  发布前的人工/CI 检查，记在阶段 4 的「依赖记录」验收里。

## QtAVPlayer 媒体适配层

`Qt6::MultimediaQuickPrivate` 是本仓库唯一的 Qt 私有模块依赖，它存在的唯一理由是
`third_party/QtAVPlayer` 的 `QT_AVPLAYER_MULTIMEDIA` 帧桥。为了让这条私有依赖可控，
**只有下面这些文件允许 `#include <QtAVPlayer/…>`**；守卫会扫描整棵 `src/` 树逐一比对。

| 文件 | 职责 |
| --- | --- |
| `src/preview/stage_media/PreviewStageMediaHost.cpp` | 适配层入口：PV/BG 播放器生命周期 |
| `src/preview/stage_media/PreviewStageMediaHost_Backend.cpp` | 后端选择与帧回调 |
| `src/preview/stage_media/PreviewStageMediaHost_Media.cpp` | 媒体装载与轨道选择 |
| `src/preview/stage_media/PreviewStageMediaHost_Playback.cpp` | 播放/暂停/seek |
| `src/preview/stage_media/PreviewStageMediaHost_Diagnostics.cpp` | 解码诊断计数 |
| `src/preview/stage_media/PreviewStageMediaHost_Timeout.cpp` | 解码超时与 EOF 归因 |
| `src/preview/stage_media/PreviewSharedD3D11Device.cpp` | Windows D3D11VA 共享设备发布 |

**后续项（阶段 4 之后，未排期）**：改用公共 `QtMultimedia` / `QVideoSink` API，去掉
`Qt6::MultimediaQuickPrivate`。前置条件是 QtAVPlayer 的帧桥不再需要 `qsgvideonode` 私有头，
或本仓库自带一份公共 API 的帧转换。在那之前，Qt 版本锁与本表就是这条私有依赖的全部约束。
