# MiaCode

Qt 6 / C++ / QML 谱面编辑、预览和导出工具。

## 代码边界

- `src/app/ui/`：QML 与界面模型；复用 `components/` 控件和 `theme/Theme.qml`。
- `src/app/services/`：共享服务与端口；ChartWorkspace 管理文档、revision 和保存点；`PreferenceDocument` 持久化偏好。
- `src/app/runtime/`：Session 装配领域宿主；PlaybackCoordinator 管理播放；延迟检测在 `runtime/latency/`。
- `src/core/chart/`：文档、解析、变换与音符模型；`src/core/analysis/`：Muri 与分析流水线；
  `src/core/scene/`、`src/core/video/`：预览与导出共享的场景数学和配置。
- `src/audio/`（BASS 后端在 `audio/bass/`）、`src/preview/`（舞台媒体在 `preview/stage_media/`）、`src/timeline/`：音频、QSG 预览、时间轴。
- `src/export/`：视频/封面导出与导出会话；`src/media_tools/`：PV 压缩、ZIP、网络；`src/common/`：日志、诊断等基础设施。
- `src/tools/`：规格；`src/devtools/`：命令行诊断入口。
- 每个目录属于一个静态库，库与依赖方向见 `docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md`；
  include 写成以 `src` 为根的完整路径，下层库只经端口取得偏好等宿主能力。

## 开发约定

- 谱面或时间语义变化需核对解析、时间轴、预览、音频、导出和 Muri。
- 异步结果校验 revision、generation 和 sequence；状态归所属服务或宿主管理。
- 预览和导出复用 QSG 场景；导出动画使用显式帧时间。
- 诊断使用 DebugLog 和 `--debug`；环境开关登记于 `docs/ops/DEBUG_INDEX.md`。
- 构建使用 Release，复用已配置目录；同目录同时运行一个构建，在低性能设备上仅允许并发上限 4。
- `MIACODE_BUILD_DEV_TOOLS=ON` 启用诊断工具和 Spec，定义位于 `cmake/devtools/`；库定义在 `cmake/MiaCodeModules.cmake`。

## 工作方式

- 从任务入口和相关调用链定位，信息足以决策时开始实现。
- 优先复用现有实现，但允许适当规模重构，防止出现补丁叠补丁；抽象和兼容处理须有实际使用场景。
- 验证围绕受影响契约展开；通过后结束，出现新证据时扩大检查范围。
- 方案分歧按代码依据和改动成本判断，影响产品决策时需确认。

## Git 维护

- 一个提交表达一个完整改动；提交前检查暂存差异和格式。
- 标题沿用仓库格式与语言：`type(scope): description`，scope 按需使用；描述实际改动，默认省略正文，禁止代理工具添加额外署名。
- 优先线性历史：更新使用 fast-forward，本地未发布提交按需 rebase，减少 merge 提交。
- 共享或已发布提交保持历史；改写历史、强制推送和分支删除前先确认。

## 入口

- [模块与同步关系](.agents/skills/miacode-dev-guide/SKILL.md)
- [构建](.agents/skills/miacode-concurrent-build/SKILL.md)
- [界面设计](.agents/skills/qt-ui-design/SKILL.md)、[布局诊断](.agents/skills/qt-ui-layout-pitfalls/SKILL.md)
- [模块分层](docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md)
- [当前规范](docs/INDEX.md)、[测试目标](cmake/devtools/specs/)、[平台脚本](scripts/README.md)

技能位于 `.agents/skills/`。
