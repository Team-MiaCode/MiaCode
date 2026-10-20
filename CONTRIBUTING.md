# Contributing to MiaCode

贡献前阅读 [README.md](README.md)、[代理与开发规则](AGENTS.md)和[文档索引](docs/INDEX.md)。
模块归属与复用入口见 [.agents/skills/miacode-dev-guide/SKILL.md](.agents/skills/miacode-dev-guide/SKILL.md)。

## 开发与验证

依赖、平台构建和打包入口见 [scripts/README.md](scripts/README.md)。
使用 Release 和适用的构建目录；开发者工具与规格通过 `MIACODE_BUILD_DEV_TOOLS=ON` 启用。

规格位于 `src/tools/<domain>/`，通过 `cmake/devtools/specs/` 的 `miacode_add_dev_tool(... TEST ...)` 注册。
目标与依赖见 `cmake/devtools/specs/`。
PR 描述说明用户可见变化、同步范围、验证证据和影响验收的限制。

## 文档与技能

共享规则维护于 `AGENTS.md`，技能维护源是 `.agents/skills/`。

构建产物、日志与本地实验资料留在忽略目录。
