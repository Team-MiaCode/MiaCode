---
name: miacode-concurrent-build
description: 查找 MiaCode 构建目录、目标、平台脚本与打包入口，适用于构建和编译命令说明。
---

# MiaCode 构建入口

Release、并发上限和构建目录使用约定见 [AGENTS.md](../../../AGENTS.md)。

- 用户指定目录时使用该目录；其他情况查看已有 `CMakeCache.txt` 的生成器、Qt 路径和开发工具开关。
- 应用目标为 `MiaCode`；Windows 启动器目标为 `MiaCodeLauncher`。
- 诊断目标定义见 `cmake/devtools/CliTools.cmake`；Spec 名称见 `cmake/devtools/specs/`。
- Windows 构建入口为 `scripts/build/build-win.ps1`；macOS 本地入口为 `scripts/build/build-macos-local.sh`。
- 打包入口为 `scripts/build/package.py`、`scripts/build/package-win.ps1` 和 `scripts/build/package-mac.sh`，参数以脚本声明为准。
- CMake preset 名称从 `CMakePresets.json` 读取。依赖配置见 `scripts/README.md`。

在已有依赖配置的目录中启用开发工具：

```sh
cmake -S . -B <build-dir> -DMIACODE_BUILD_DEV_TOOLS=ON
cmake --build <build-dir> --config Release --target <target> --parallel 4
```

全量构建省略 `--target`。单配置生成器的 Release 由配置阶段的 `CMAKE_BUILD_TYPE` 决定。
输出位置按生成器、平台和目标属性查找。

Visual Studio 的 MSBuild 项目并行度与 MSVC `/MP` 会叠加；共享头文件变化会触发多个翻译单元重编译。
