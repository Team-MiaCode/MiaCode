# Scripts

[中文](README.md) | [English](README_EN.md)

本目录只保留可公开复现的构建、发布、资产和诊断脚本。一次性分析脚本和旧 A/B 诊断入口保留为维护者本地工具，不进入公开跟踪。

## 目录结构

| 目录 | 内容 |
|---|---|
| `build/` | Windows/macOS 构建、打包与出包校验入口 |
| `debug/` | Windows/macOS 调试/诊断启动入口 |
| `ffmpeg/` | FFmpeg 运行时、开发 SDK 获取脚本，以及 decode-only 裁剪工具链 |
| `assets/` | 资产生成和字体裁剪辅助脚本 |
| `api/` | Net 应用操作目录、Schema、OpenAPI 与客户端类型的生成和独立标准校验 |

## Net API 规范生成

`python scripts/api/generate_net_api.py` 从 `tools/net-api/operations.json` 与 `schemas.json` 生成内嵌 C++ 目录、OpenAPI、TypeScript 类型和接口说明；加 `--check` 检查漂移。独立标准校验在 `tools/net-api` 执行 `npm ci --ignore-scripts` 和 `npm run validate`。生成器仅使用 Python 标准库，校验工具的 Node 依赖不进入产品运行时。

当前可用范围及后续新增能力规则见 [Net 接口规范化计划](../docs/specs/net/NET_API_STANDARDIZATION_PLAN_ZH.md)。生成出的 HTTP 路由不表示网关已经开放。

## 构建与打包

Windows:

```powershell
# MSVC x64：生成器由 vswhere 解析，需已装 Visual Studio 与 Windows SDK
# x64 默认用 ffmpeg/trim 从源码构建 decode-only 预览 SDK（体积从约 150 MB 降到约 20 MB，需 MSYS2）；
# 跳过裁剪加 -SkipTrim
powershell -ExecutionPolicy Bypass -File .\scripts\build\build-win.ps1 -Toolchain msvc -BuildDir build-msvc

# MSVC arm64：原生 arm64 主机，走 ARM64 平台参数与 Qt 的 arm64 包
powershell -ExecutionPolicy Bypass -File .\scripts\build\build-win.ps1 -Toolchain msvc-arm64 -BuildDir build-msvc-arm64

# 构建已完成时单独打包（-Arch 与构建时一致）
powershell -ExecutionPolicy Bypass -File .\scripts\build\package-win.ps1 -QtRoot <QtRoot> -BuildDir <BuildDir> -Arch x64
```

`build-win.ps1` 依次执行：导出用 `ffmpeg.exe`、预览用 FFmpeg
dev SDK（x64 走裁剪构建，`-SkipTrim` 关闭）、Qt 定位、CMake 配置与构建、调用 `package-win.ps1` 打包。两条 FFmpeg 链与打包
链都按 `-Toolchain` 推导出的目标架构选择各自的目录：`third_party\ffmpeg\windows\<win64|winarm64>\`
与 `third_party\bass\bin\<win64|winarm64>\`，包名分别为 `MiaCode_<version>_win_x64` 与
`MiaCode_<version>_win_arm64`（设 `MIACODE_PACKAGE_CHANNEL` 时在版本后插入渠道段）。arm64 不随包分发 `bass_aac.dll`（上游只提供 x86/x64 版本），
音频后端对缺失插件已有存在性检查。

Qt 定位顺序：`-QtRoot` → `C:\Qt\<版本>\<架构目录>` → `.qt\<版本>\<架构目录>` →
`QT_ROOT_DIR`/`Qt6_DIR` 环境变量 → 都未命中时由 `build/provision-qt.ps1` 从 Qt 仓库下载。
后者自适应上游两种仓库目录布局（6.11 前的扁平目录与 6.11 起的按架构分目录），每个归档按
发布方 `.sha1` 校验，不依赖 aqtinstall。

`package-win.ps1` 在需要时自动重建 `MiaCode` 与 `MiaCodeLauncher`，把 MSVC 运行库与裁切后的
FFmpeg 运行库放入 `app/`，按内容契约断言必需项，产出 7z 归档；
单配置生成器下还会校验 `CMAKE_BUILD_TYPE` 与 `-Config` 一致。

预览用 FFmpeg SDK 可由 `ffmpeg/trim/build-trimmed-ffmpeg.ps1` 产出裁切版，替代
`ffmpeg/ensure-windows-ffmpeg-dev.ps1` 下载的全量 SDK。

Qt 版本与模块、两套 MSVC 目标架构、包内容清单和归档格式位于
`build/windows-toolchain.psd1`。两个架构使用各自的构建目录。

macOS:

本地出包，复用机器上已装的 Qt、FFmpeg dev SDK 与导出用 `ffmpeg`：

```bash
bash scripts/build/build-macos-local.sh
```

CI 出包，使用 Qt 6.11.1 与 FFmpeg 8.1.2 预览 SDK：

```bash
bash scripts/build/build-macos-ci.sh
```

两个入口都把 Release 构建与包装配交给 `package-mac.sh`；需要手工控制时可直接
调用它，例如 `QT_ROOT="$HOME/Qt/6.11.1/macos" bash scripts/build/package-mac.sh`。
CI 入口读 `QT_VERSION`、`QT_MODULES`、`MIACODE_PYTHON_VENV_DIR`；本地入口读
`QT_ROOT`（按 `QT_VERSION` 依次探测 `.qt/` 与 `$HOME/Qt/`）、`BUILD_DIR` 与
`MIACODE_FFMPEG_DEV_DIR`。设置 `MIACODE_PACKAGE_CHANNEL` 会在包名里插入渠道段。

输出位于 `dist/`。指定单一 `arm64` 或 `x86_64` 架构时，打包流程会在
`macdeployqt` 后裁掉 Qt Framework/插件中的另一架构切片，并在重新签名前验证
包内所有 Mach-O 均包含且只包含目标架构。可设置
`MIACODE_THIN_MACOS_APP=OFF` 生成保留 Qt universal 二进制的对照包。

macOS 的 QtAVPlayer 预览解码还需要 FFmpeg dev SDK，位于仓库本地的
`third_party/ffmpeg/macos/dev/`，用 `bash scripts/ffmpeg/ensure-macos-ffmpeg-dev.sh`
生成（已存在且校验通过时直接复用）；打包仅复制其中必需的六个 dylib，
不会查找或复制 Homebrew 依赖。也可用 `MIACODE_FFMPEG_DEV_DIR` 显式指定兼容 SDK。

## 开发者工具

`MIACODE_BUILD_DEV_TOOLS=ON` 启用命令行诊断工具和 Spec。
程序入口位于 `src/devtools/`，CMake 定义位于 `cmake/devtools/CliTools.cmake`，
注册入口为 `cmake/devtools/DevTools.cmake`。

| 程序 | 用途 |
|---|---|
| `miacode_simai_dump` | 输出谱面解析 JSON |
| `miacode_muri_dump` | 输出无理分析 JSON |
| `miacode_audio_probe` | 检查音频变速播放 |
| `miacode_latency_offset_batch` | 批量评估谱面偏移检测 |

Spec 按领域登记于 `cmake/devtools/specs/`，清单见
[`SPEC_CATALOG.md`](../docs/tests/SPEC_CATALOG.md)。

## 其他脚本

- `debug/Start_MiaCode_Debug.bat`：发布包内唯一 Windows 调试启动入口。
- `debug/Start_MiaCode_Debug.command`：发布包根目录内的 macOS 调试启动入口；双击后以 `--debug` 启动 `MiaCode.app`，并将日志写入发布包根目录的 `logs/`。
- `debug/Start_MiaCode_SoftwareVideoDecode.bat`、`debug/Start_MiaCode_QtPluginDiag.bat`：公开保留的支持诊断入口，不随 Windows 发布包分发。
- `ffmpeg/ensure-windows-ffmpeg.ps1`、`ffmpeg/ensure-macos-ffmpeg.sh`、`ffmpeg/ensure-linux-ffmpeg.sh`：获取导出用独立 `ffmpeg`。
- `ffmpeg/ensure-macos-ffmpeg-dev.sh`：构建 macOS QtAVPlayer 预览解码用的 FFmpeg 8.1.2 SDK。
- `ffmpeg/ensure-windows-ffmpeg-dev.ps1`：获取 Windows QtAVPlayer 预览解码开发 SDK。
- `ffmpeg/trim/`：构建 Windows decode-only FFmpeg dev SDK 的裁剪工具链。
- `assets/subset_hud_font.py`：HUD 字体子集生成，详见 `assets/README_font_subset.md`。
- `assets/gen_same_lane_v_slides.py`：生成同轨 V 型 slide 参考数据。

## ScintillaQuick 编辑器依赖

`third_party/ScintillaQuick` 使用 Git submodule，固定提交
`bfa6ae93315942b591878b0e709e1363bc07bfd6`，来源为
[imakris/ScintillaQuick](https://github.com/imakris/ScintillaQuick)。检出仓库时执行：

```sh
git submodule update --init --recursive
```

顶层 CMake 通过 `add_subdirectory` 和 `ScintillaQuick::ScintillaQuick` 集成。
依赖的示例、基准和测试构建开关关闭；应用使用 `MiaCode.UI` 中的
`ScintillaEditor` 类型。BSD-2-Clause 许可及 Scintilla 许可随依赖保留。

`ScintillaDocumentAdapter` 保留各难度的原生文档、撤销历史与视口，并转换
服务层 UTF-16 位置和 Scintilla UTF-8 字节位置。`ScintillaDslStyler` 使用
container styling、marker 和 indicator 表达语法、书签、诊断及播放位置。
`ScintillaEditorBridge` 对接 DocumentModel、EditorSyncController 与 AnalysisModel，
在 IME 预编辑期间保留服务层的已提交正文，导航携带难度、revision 与 sequence。
