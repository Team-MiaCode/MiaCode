<p align="center">
  <img src="resources/icons/app.png" alt="MiaCode avatar" width="128">
</p>

<h1 align="center">MiaCode</h1>

<p align="center">
  <a href="https://miacode.net/"><img src="https://img.shields.io/badge/%E5%AE%98%E7%BD%91-miacode.net-3178C6?style=for-the-badge" alt="官网"></a>
  <a href="https://github.com/Team-MiaCode/MiaCode/releases/latest"><img src="https://img.shields.io/github/v/release/Team-MiaCode/MiaCode?style=for-the-badge&amp;label=Release&amp;logo=github&amp;color=24292F" alt="Latest release"></a>
  <a href="https://github.com/Team-MiaCode/MiaCode/actions/workflows/package.yml?query=branch%3Adev"><img src="https://img.shields.io/badge/Nightly-CI-2088FF?style=for-the-badge&amp;logo=githubactions&amp;logoColor=white" alt="Nightly CI"></a>
  <a href="https://github.com/Team-MiaCode/MiaCode/stargazers"><img src="https://img.shields.io/github/stars/Team-MiaCode/MiaCode?style=for-the-badge&amp;label=Stars" alt="GitHub stars"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue?style=for-the-badge" alt="Source code license: GPLv3"></a>
</p>

<p align="center">
  <a href="README.md">中文</a> | <a href="README_EN.md">English</a>
</p>

MiaCode 是一款基于 Qt 6 / C++ / QML 的 maimai 谱面创作工具，集成编辑、实时预览、语法与无理检测、视频导出和封面创作，支持 Windows、Apple 芯片的 macOS 和 Linux。

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/screenshots/main-dark.png" alt="MiaCode 深色主题多标签页" width="640"></td>
    <td align="center" width="50%"><img src="resources/screenshots/main-light.png" alt="MiaCode 浅色主题、高效侧栏与独立预览" width="640"></td>
  </tr>
  <tr>
    <td align="center">深色主题</td>
    <td align="center">浅色主题</td>
  </tr>
</table>

## 快速开始

### 下载

<p align="center">
  <a href="https://miacode.net/download.html"><img src="resources/readme/download-website-zh.svg" alt="官网下载" width="264" height="88"></a>
  <a href="https://github.com/Team-MiaCode/MiaCode/releases"><img src="resources/readme/download-release.svg" alt="GitHub Releases" width="264" height="88"></a>
  <a href="https://github.com/Team-MiaCode/MiaCode/actions/workflows/package.yml?query=branch%3Adev"><img src="resources/readme/download-nightly.svg" alt="Nightly CI" width="264" height="88"></a>
</p>

提供 Windows 和 macOS 安装包。

Linux 用户可[从源码构建](#构建)。

- **Windows**：双击解压目录中的 `MiaCode.exe` 启动。
- **macOS**：双击 `MiaCode.app` 启动，也可将其拖入“应用程序”文件夹。若系统显示安全提示，可在解压目录打开终端，执行 `xattr -dr com.apple.quarantine "MiaCode.app"` 后启动。

具体使用方法见[快速开始](https://miacode.net/miacode/quick-start)。

## 功能介绍

### 特色

- 多组件宽度自由调节，编辑器与预览区面板可左右交换重排。

- 深、浅色主题与中 / 英 / 日三种语言支持，可跟随系统切换。

- 键入修改实时更新，无需处于播放模式，可随时拖拽进度条查看配置。

- 多难度标签页、源码概览标尺与自定义补全候选。

- 预览窗口可独立分离，界面支持缩放。

### 语法与无理

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/screenshots/syntax.png" alt="语法检查结果" width="640"></td>
    <td align="center" width="50%"><img src="resources/screenshots/muri.png" alt="无理检测结果" width="640"></td>
  </tr>
  <tr>
    <td align="center">语法检查</td>
    <td align="center">无理检测</td>
  </tr>
</table>

支持谱面语法检查与谱面无理配置检测，可快速跳转到指定行。

### 视频与封面

#### 导出

<p align="center">
  <img src="resources/screenshots/video-export.png" alt="视频导出界面" width="1280">
</p>

导出包含片头与全景 PV 的预览视频。

支持自定义导出区间，或直接在编辑器内选择谱面段落区间并套用。

支持区间试听、MP4 / WAV / MP4 + WAV 输出，以及批量导出视频与预设封面。

#### 封面

<p align="center">
  <img src="resources/screenshots/cover-editor.png" alt="封面编辑界面" width="1280">
</p>

导出用于发布谱面视频的平台封面。

支持自定义字体、背景等样式，也可以叠加谱面帧截图，用于展示配置。

支持难度卡、谱面帧、图片与文字图层，以及布局预设。

### Net 谱面下载与上传

<p align="center">
  <img src="resources/screenshots/net-download.png" alt="Net 谱面查询结果与在线预览入口" width="1280">
</p>

按用户 ID、Tag、歌曲名与上传时间筛选 Majdata Net 谱面，勾选后批量下载到指定目录，支持额外生成 ZIP 压缩包。

支持在线预览、下载 PV，以及登录 Majdata Net 批量上传本地谱面。

### 更多实用功能

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/screenshots/completion.png" alt="自动补全时值" width="640"></td>
    <td align="center" width="50%"><img src="resources/screenshots/normalize.png" alt="谱面整理" width="640"></td>
  </tr>
  <tr>
    <td align="center">时值自动补全</td>
    <td align="center">谱面整理</td>
  </tr>
  <tr>
    <td align="center" colspan="2"><img src="resources/screenshots/media-tools.png" alt="音视频工具" width="1280"></td>
  </tr>
  <tr>
    <td align="center" colspan="2">音视频工具</td>
  </tr>
  <tr>
    <td align="center" colspan="2"><img src="resources/screenshots/latency.png" alt="BPM 与延迟检测" width="960"></td>
  </tr>
  <tr>
    <td align="center" colspan="2">BPM 与延迟检测</td>
  </tr>
</table>

- 自定义背景

- 输入法禁止与全角字符转换
- 书签跳转段落
- 快捷编写 Touch 音符
- 拖入音频创建谱面
- 显示选区拍数
- 重置摆键到 1 号
- 自动补全时值
- 一键谱面整理

- 音视频工具
- BPM 与延迟检测

<details>
<summary><h2 id="构建">构建</h2></summary>

### 依赖

- CMake 3.21+
- C++20 编译器
- Qt 6.10+，包含 Qt Quick、Quick Controls 2、Multimedia、Shader Tools、Linguist Tools，以及 Gui 与 Multimedia 私有开发头文件

更详细的打包说明见 [scripts/README.md](scripts/README.md)。

### Windows

手动安装以下构建工具：

- Visual Studio 2022 或 Build Tools 2022，选择“使用 C++ 的桌面开发”
- CMake 3.21+
- Python 3（包含 pip）

使用一键脚本自动安装 Qt、准备依赖、构建并打包：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build\build-win.ps1 -Toolchain msvc -BuildJobs 4
```

ARM64 主机使用 `-Toolchain msvc-arm64`，并选择独立构建目录；参数见 [scripts/README.md](scripts/README.md)。

### macOS

使用 Apple 芯片 Mac，系统版本为 macOS 13 或以上。在终端安装 Xcode Command Line Tools，并通过 [Homebrew](https://brew.sh/) 安装 CMake 和 Python 3：

```bash
xcode-select --install
brew install cmake python
```

安装 Qt 6.10+（包含 Multimedia 和 Shader Tools），准备预览 SDK 与导出用 FFmpeg，然后构建并打包：

```bash
python3 -m pip install aqtinstall
python3 -m aqt install-qt mac desktop 6.11.1 clang_64 --outputdir .qt \
  --modules qtmultimedia qtshadertools
bash scripts/ffmpeg/ensure-macos-ffmpeg-dev.sh
bash scripts/ffmpeg/ensure-macos-ffmpeg.sh
bash scripts/build/build-macos-local.sh
```

已有 Qt 时可设置 `QT_ROOT`；本地脚本复用依赖，输出位于 `dist/`。

### Linux（适用于高级用户）

Linux 无打包脚本，需要手动安装 CMake 3.21+、C++20 编译器、pkg-config 和 Qt 6.10+（包含 Qt Quick、Quick Controls 2、Multimedia、Multimedia 的私有开发头文件、Shader Tools 和 Linguist Tools），以及 FFmpeg、VA-API、DRM、OpenGL / EGL 的开发库。FFmpeg 开发库需包含 `libavfilter`、`libavcodec`、`libavformat`、`libavutil`、`libswresample` 和 `libswscale`。

在仓库根目录执行以下命令，将 `[/path/to/Qt/6.x/gcc_64]` 替换为 Qt 安装目录：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="[/path/to/Qt/6.x/gcc_64]"
cmake --build build --target MiaCode --parallel 4
./build/bin/MiaCode
```

音视频处理与导出使用独立的 `ffmpeg` 程序，可手动安装支持 H.264 / AAC 编码的 FFmpeg。

</details>

<details>
<summary><h2 id="仓库结构">仓库结构</h2></summary>

- [src](src)：应用源码
- [assets](assets)：运行资源、素材与生成数据
- [resources](resources)：Qt resource collection
- [scripts](scripts)：构建与维护脚本
- [third_party](third_party)：第三方依赖
- [docs](docs)：文档
- [samples](samples)：规格测试示例资源

</details>

<details>
<summary><h2 id="许可证">许可证</h2></summary>

项目代码采用 [GPLv3](LICENSE)，第三方授权见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

</details>

## 维护与反馈

其他问题可以在 [Issues](https://github.com/Team-MiaCode/MiaCode/issues) 中查看，也可以在那里分享使用过程中的错误与异常。

提交 [Issue](https://github.com/Team-MiaCode/MiaCode/issues) 时，说明错误或异常行为的现场状态、此前操作、复现步骤、MiaCode 版本、运行环境和问题现象，并附上相关信息。

日志和调试信息对我们来说很重要，收集方式见[调试指南](docs/ops/DEBUG_INDEX.md)。

代码和文档改进欢迎通过 Pull Request 分享，较大的功能或架构调整可以先在 Issue 中交流。

## 致谢

感谢 [Minepig/MaiMuriDX](https://github.com/Minepig/MaiMuriDX) 提供的无理检测功能参考。

感谢 [gfdfdxc/maimai-transition](https://github.com/gfdfdxc/maimai-transition) 提供的片头动画参考。

感谢 [Majdata Net](https://majdata.net/) 提供的自制谱面社区。

感谢 [MaiViewer](https://www.maiviewer.net/) 提供官方谱面抄谱下载站。

特别感谢 hitomi 老师无偿提供 MiaCode logo 绘制。

感谢内部测试时期给出建议、复现问题和协助调试的朋友们，名单见 [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)。

## 社群

欢迎加入 MiaCode 官方 QQ 交流群：1095435375

<p align="center">
  <img src="resources/community/qq-group.png" alt="MiaCode QQ 群二维码" width="360">
</p>
