---
lifecycle: working
owner: src/app/services/api
canonical_id: net.api-standardization-plan
last_verified: 2026-10-10
code_anchors: ["tools/net-api/operations.json", "tools/net-api/schemas.json", "scripts/api/generate_net_api.py", "src/app/services/api/ApiCatalog.cpp", "src/app/services/api/ApiDispatcher.cpp", "packages/miacode-client/src/client.ts"]
---

# Net 接口规范化计划与当前交付

统一业务契约以稳定 operationId 为入口。Qt/QML、CLI、HTTP 网关及各平台适配器使用相同请求、任务和结果规则；平台差异放在 transport、storage、credential、interaction 端口。

当前版本为 API **1.0**、Schema **1.0.0**，登记 **34 个操作、74 个 Schema**。目录、OpenAPI、SDK 类型及公共分发器同步生成。**20 个操作接入桌面宿主内部**，覆盖账户、上传、下载、预览、查询、文档快照和任务基础；14 个通用服务及适配器操作按计划登记。实际能力由运行时处理器、平台端口和主体权限共同决定。

当前核对对象是桌面 v2 的 `dev` 分支，核对起点 HEAD 为 `663d14d34150b8cb3c06b4430140851fc72b4495`；提交前同步至 `36dc7d08`，封面和元数据界面的交叉改动纳入构建验证。旧评估中的固定提交保留为历史调查证据。MiaCode Mobile 是独立 Android 项目，其文档、迁移来源和 P5 验收不用于判定桌面 v2 状态；Mobile 原定用户验收后同步顺序保持有效。

## 1. 权威来源与生成产物

| 产物 | 维护规则 |
| --- | --- |
| `tools/net-api/operations.json` | 人工维护 operationId、owner、路由、权限、授权规则、同步/异步、幂等、敏感字段、版本、验收映射和适配器状态 |
| `tools/net-api/schemas.json` | 人工维护请求、响应、任务完成结果及公共 DTO，JSON Schema Draft 2020-12 |
| [实施契约](NET_API_CONTRACT_ZH.md) | 规定领域语义，包括过滤、账号、文件发布、授权、重试、文档身份和兼容性 |
| [生成操作目录](generated/NET_OPERATION_CATALOG_ZH.md) | 生成 34 项用途、参数/结果、路由、幂等及验收映射，不能独立修改 |
| [OpenAPI](generated/net-openapi.json) | 生成 HTTP 契约，OpenAPI 3.1.1，不能独立修改 |
| `src/app/services/api/generated/NetApiCatalogData.h` | 生成产品内嵌目录与 Schema，发布时不依赖外部文件 |
| `packages/miacode-client/src/generated.ts` | 生成 DTO、OperationMap 与路由元数据 |
| `net-migration-inventory.json` | 保留 50 项功能/20 组验收的历史调查映射，inventory_only 不用于报告当前 capability |

描述格式依据 [OpenAPI 3.1.1](https://spec.openapis.org/oas/v3.1.1.html) 和 [JSON Schema Draft 2020-12](https://json-schema.org/draft/2020-12/json-schema-validation)。生成器校验目录、引用、路由、权限元数据、敏感字段和生成文件的一致性。

## 2. 接口分组及当前状态

| 分组 | 数量 | 当前内部可调用 | 后续实现 |
| --- | ---: | --- | --- |
| 能力与文档 | 2 | `api.capabilities`、`document.snapshot` | 按宿主文档服务装配 |
| 平台与查询 | 4 | `net.providers.list`、`net.probes.create`、`net.queries.create`、`net.queries.results` | 补真实平台与产品验收 |
| 下载与上传 | 3 | `net.downloads.create`、`net.uploads.scan`、`net.uploads.create` | 用户执行上传验收 |
| 账号 | 3 | `net.accounts.login/list/logout` | 用户执行账户验收；其他平台凭据适配 |
| 在线预览 | 3 | `net.previews.prepare/open/release` | 按平台补媒体运行验收 |
| 设置 | 2 | — | `net.settings.get/update` |
| 任务 | 8 | `jobs.list/get/items/events/cancel` | `jobs.resume/retry/diagnostics` |
| 文件 | 2 | — | `files.grants.request`、`files.artifacts.content` |
| 通用网络与代理 | 5 | — | `network.targets.request`、`network.http.fetch/download`、`network.proxy.get/set` |
| 网关配对 | 2 | — | `gateway.pairings.create/exchange` |

`net.*` 是在线谱面业务，`network.*` 是通用 GET、目标授权和请求域代理。任务、文件、能力目录、配对为共享应用服务；文档编辑、播放、导出仍由各自领域 owner 管理。

运行时 capability 根据真实处理器和主体权限报告 `available`，并分别报告 `adapters.internal/http/cli`。平台实测与实现状态分开记录；Windows 离线测试和 Release 构建不能推出其他平台已交付。Android/iOS/WASM 需要各自的文件、凭据、交互和网络适配器。

QML 新功能通过现有标签页打开。工具箱与工具菜单以“谱面下载”“谱面上传”打开 `net-download`、`net-upload`，重复打开复用对应标签；文档替换与关闭保留独立 Net 标签。任务归服务管理，关闭标签不取消任务；用户取消通过 `jobs.cancel` 执行。页面的主题、日期选择器、表格与操作控件复用 QML 公共组件。

## 3. 已落实的协议规则

- 请求拒绝未知字段、错误类型、非法枚举、重复批次引用、uint64 数字/溢出、非法日期与时区。默认值和归一化在服务边界执行：trim、Tag 前缀、系统时区转 IANA、反向日期范围；密码不 trim。
- 可信适配器提供 principal/scopes/requestId，这些身份不接受请求 JSON 覆盖。执行前检查权限；任务、查询与游标再检查主体所有权。
- 同步响应为 200，任务接收为 202；统一成功/错误 envelope。同步结果与成功任务完成结果也经 Schema 检查。
- jobId/itemId/queryRef 不包含本机路径。64 位数据使用十进制字符串。查询分页与任务分页绑定不可变快照；item.updated 带结构化 item 状态。
- 查询条件使用 AND，候选缓存与结果快照分离，completeness 为 unknown。任务取消保留已完成项目，等待任务 owner 结束。
- 未实现项返回 capability.unavailable。需要持久幂等、安全上传恢复、文件/目标授权及配对的操作，在这些条件完成前保持未开放。

当前任务及可选查询幂等记录仅在进程内保留，capability 报告 process_lifetime。24 小时持久账本和崩溃后上传 outcome_unknown 恢复是后续开放门槛；现有内存记录不提供重启恢复保证。

## 4. 实施顺序与完成门槛

| 工作包 | 状态 | 交付条件 |
| --- | --- | --- |
| A：目录与协议 | 本轮完成 | 34 项目录、74 个 Schema、OpenAPI、SDK 类型、公共校验/分发、生成一致性与行为测试 |
| B：查询与任务基础 | 已接入 | 查询/过滤/时区/排序/缓存/探测、异步传输、任务分页/事件和查询标签；补真实服务与产品验收 |
| C：文件与下载 | 桌面接入 | 内部目录授权、流式 staging/manifest、PV、目录/ZIP 发布、取消、恢复和偏好；固定样本验证 TC06–TC08 的资源契约 |
| D：账号与上传 | 桌面接入 | Cookie 隔离、Windows Credential Manager、冻结计划和文件副本、共享限流及结果待确认；源码与编译核对，用户执行 TC11–TC15 的运行验收 |
| E：在线预览 | 桌面接入 | 主体/版本/TTL 缓存、独立打开、文档身份与离开决策、origin 和另存为；固定样本及文档文件服务验证 TC09–TC10 的资源和保存契约 |
| F：公共适配器 | 待实施 | HTTP/CLI、配对、持久幂等、资源授权、通用 GET/下载、DNS/重定向校验、请求域代理；TC16–TC18 |
| G：平台与交付 | 待实施 | 支持平台的 TLS/路径/凭据/打包证据、真实浏览器和产品验收；TC19–TC20 |

业务处理器完成验收后开放对应 HTTP/CLI 入口。网关基础可独立测试，capability 仍按业务处理器和平台端口报告状态。公共 jobs.resume/retry 需要继承原业务权限、验证解除条件并接入持久幂等，不能直接开放当前内部恢复辅助方法。

## 5. 后续新增能力规则

1. 确定用户行为与领域 owner，按 `domain.resource.action` 登记稳定 operationId；业务 ID 不取 QML 页面名或上游 URL。
2. 更新 operations.json 与 schemas.json，登记参数/响应/任务结果、权限与动态授权、幂等、敏感字段、版本、平台、适配器和验收。同步扩展功能/验收清单。
3. 领域服务使用 typed ports，落实异步 owner、取消、进度、epoch/sequence、终态和结果不明；资源引用每次核验授权。
4. 补行为测试与平台证据，运行生成器和一致性守卫；未实现项保持 planned/unavailable。
5. 可选字段/操作扩展提升 minor；删除或改变字段、权限、默认语义、状态含义使用新 major。弃用登记 replacement/sunset 与迁移说明。
6. 审阅最终差异后执行受影响测试、Release 构建和模块分层守卫；提交/推送遵循仓库规则。

```text
python scripts/api/generate_net_api.py
python scripts/api/generate_net_api.py --check
python scripts/governance/module_layering.py
python scripts/governance/docs_index.py --check
```

C++ 校验器实现本目录明确使用的 Schema 关键字子集；生成器拒绝未支持关键字。新增复杂约束必须同步补产品校验和测试，不自称任意 JSON Schema 实现。

SDK 见 [客户端说明](../../../packages/miacode-client/README.md)。它支持注入式 transport、HTTP 编码、任务等待、分页、AbortSignal 和错误 envelope；SDK 类型不替代宿主授权和参数校验。

独立标准验证工具位于 `tools/net-api`：运行 `npm ci --ignore-scripts`、`npm run validate`，通过 Ajv 与 OpenAPI parser 校验 Schema/引用/路由。它仅为开发依赖，不进入 Qt 产品运行时。

## 6. 验证记录（2026-10-10）

验证入口采用 `build-devtools/desktop-qt610` Release 构建。下载和预览使用本地 GET 固定样本；账户、目录扫描和上传使用源码核对、Schema 检查及编译证据，运行验收由用户执行。

| 检查 | 范围 |
| --- | --- |
| MiaCode、MiaCodeLauncher | C++、QML、翻译资源与 Windows 凭据库链接 |
| net_download_flow_spec | 公共下载分发、幂等、资源与 ZIP/PV、空谱面、重试、阻断、冲突、取消、预览缓存与版本、主体授权、编译翻译文案 |
| net_query_rules_spec、net_http_transport_spec、net_provider_spec | 过滤、日期、排序、缓存、流式 HTTP、响应分类和查询 |
| chart_workspace_spec、chart_workspace_file_service_spec | 文档状态、预览来源、另存为、保存代次及原子写入 |
| job_registry_spec、ui_text_locale_spec | 任务状态与事件、翻译目录和文案引用 |
| 生成器、JSON Schema/OpenAPI、SDK TypeScript | 34 项操作、74 个 Schema、标准引用与类型一致性 |
| 模块分层与文档索引 | 代码依赖方向与当前文档登记 |

页面布局核对采用用户截图、v1 布局代码和 QML 结构。平台媒体运行、用户交互及远端服务验收沿用验收清单。完整迁移清单保留 50 项功能的分组验收记录；本阶段目标见 [账户、上传、下载与在线预览迁移目标](NET_MIGRATION_TARGET_ZH.md)。

本次 Release 构建与 8 项定向 CTest 通过；下载固定样本产物为 7,695 字节。功能对应、源码与检查结果见 [桌面实现与验证记录](NET_DESKTOP_MIGRATION_VERIFICATION_ZH.md)。
