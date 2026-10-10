---
lifecycle: working
owner: src/app/services/net
canonical_id: net.desktop-migration-verification
last_verified: 2026-10-10
---

# 桌面 Net 迁移实现与验证记录

桌面 v2 迁移起点为 `663d14d34150b8cb3c06b4430140851fc72b4495`；提交前同步至 `36dc7d08`，核对封面和元数据界面的交叉改动。v1 功能对照为 `6573efb611056bccd046dd17bc2f4cceda78bf88`。页面结构依据用户提供的下载截图、v1 Dialog 源码以及 v2 QML 布局核对。

## 功能对应

| 清单 | 实现与行为 | 核对依据 |
| --- | --- | --- |
| F01、F43 | 工具菜单与工具箱提供“谱面下载”，独立标签页复用，中文、英文、日文文案 | MainMenu、ActivityBar、Sidebar、PageHost、ViewState、EditorTabBar；QML 编译与翻译 Spec |
| F02–F08、F11 | 账户名、Tag、标题 AND 过滤；字段解析、时区和日期边界；候选缓存、强制刷新、查询分页、连接检测 | NetQueryRules、NetProvider；查询规则和 provider Spec |
| F09、F10、F12 | 等级、时间、状态升降序及标题排序；逐项选择、全选、取消选择、目录记忆 | NetDownloadPage、NetModel；共享排序规则 Spec，页面及绑定源码核对 |
| F13–F21 | 音频、封面、谱面、可选 PV；串行流式 staging、原子发布、ZIP、容量限制、重试、阻断、取消与恢复；项目进度、字节与速度、日志 | NetHttpTransport、NetResourceOperation、NetModel；HTTP 与下载流程 Spec |
| F22–F25 | 主体和版本隔离的预览缓存、缺 PV 记录、准备与打开分离；文档身份检查、离开决策、NetPreview 来源、媒体加载与另存为 | NetResourceOperation、ApplicationContext、DocumentModel、ChartWorkspaceFileService、runtime/document；下载流程及文档 Spec、调用链核对 |


## 接口状态

机器契约包含 34 项操作与 74 个 Schema；Markdown 操作目录列出 29 项，其中 15 项为宿主内部实现。查询、下载、预览及通用任务通过 ApiDispatcher、NetService 与 NetEnginePort 调用。HTTP/CLI 等适配器状态由 capability 和 [规范化计划](NET_API_STANDARDIZATION_PLAN_ZH.md) 记录。


## 验证入口

环境为 Windows、Qt 6.10.3、MSVC 2022、Release，构建目录为 `build-devtools/desktop-qt610`。

```text
cmake --build build-devtools/desktop-qt610 --config Release --target MiaCode MiaCodeLauncher --parallel 4
ctest --test-dir build-devtools/desktop-qt610 -C Release -R "^(net_query_rules_spec|net_http_transport_spec|net_provider_spec|net_download_flow_spec|ui_text_locale_spec|job_registry_spec|chart_workspace_spec|chart_workspace_file_service_spec)$" --output-on-failure
python scripts/api/generate_net_api.py --check
npm --prefix tools/net-api run validate
npm --prefix packages/miacode-client run check
python scripts/governance/module_layering.py
python scripts/governance/docs_index.py --check
```

构建输出保存于 `build-devtools/net-migration-build.log`；测试输出由 CTest 记录于构建目录 `Testing/Temporary/LastTest.log`。

## 本次结果

- `MiaCode` 与 `MiaCodeLauncher` Release 构建通过。
- 上述 8 项 CTest 通过，合计 3.24 秒。
- 下载固定样本产物合计 7,695 字节，容量上限 65,536 字节，目录自动清理。
- 34 项操作与 74 个 Schema 生成一致性、JSON Schema Draft 2020-12、OpenAPI 3.1 和 TypeScript 检查通过。
- 模块分层、文档索引和差异格式检查通过。

## 提交前复核（2026-10-10）

同步 `origin/dev` 至 `36dc7d08` 后，默认 `MiaCode` Release 构建通过。
定向 CTest 共 12 项通过，覆盖上述 8 项、Net 页面编译、依赖清单、Slide 区域擦除和预览对象热路径。
依赖清单补充 Net 的 Qt Network/Concurrent 和下载 ZIP 使用点，
并登记 macOS 窗口材质使用的 QuartzCore。API 生成检查按文本内容比较 Windows 与其他平台的换行。
Schema/OpenAPI、TypeScript、模块分层、规格目录、文档索引和差异格式检查通过。
日期控件按内容测量日历尺寸，以窗口边界约束弹出位置；按钮重复点击切换开关，外部点击与 Esc 关闭。
