# 开发入口

共享构建与验证约定见 [AGENTS.md](../../../../AGENTS.md)。

| 内容 | 入口 |
| --- | --- |
| 平台依赖、构建与打包脚本 | `scripts/README.md`、`scripts/build/` |
| 构建目标与目录选择 | `miacode-concurrent-build` skill |
| 命令行诊断 | `src/devtools/`、`cmake/devtools/CliTools.cmake` |
| 库定义与目录 | `cmake/MiaCodeModules.cmake`、`docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md` |
| Spec 注册 | `cmake/devtools/specs/` |
| 调试开关与日志 | `docs/ops/DEBUG_INDEX.md`、`src/common/DebugLog.h` |
| 资源解析 | `src/core/video/AssetPaths.h`、`src/core/chart/ChartAssetPaths.h`、ChartMediaService |
| 翻译与偏好 | `translations/`、`src/app/services/PreferenceDocument.h` |
| 业务说明 | `docs/INDEX.md` |

Spec 使用所属领域 manifest 的 `miacode_add_dev_tool(... TEST ...)` 注册源文件与依赖。
库内代码通过 LIBS 链接所属 `miacode_*` 库，SOURCES 只列 Spec 自身和 `MiaCode` 的源文件。

技能位于 `.agents/skills/`。
