---
lifecycle: working
owner: src
last_verified: 2026-10-09
---

# Net 迁移任务包与验收清单

本清单用于实施 [迁移评估](../specs/net/NET_MIGRATION_ASSESSMENT_ZH.md) 和 [API 实施契约](../specs/net/NET_API_CONTRACT_ZH.md)。**以下 TC 项是待执行的验收要求，本次调查没有把它们标为通过。** 功能、源码和验收的机读映射见 [清单 JSON](../specs/net/net-migration-inventory.json)。

当前实施记录：已新增 net_query_rules_spec、net_http_transport_spec、net_provider_spec、job_registry_spec、net_service_spec、net_api_contract_spec、qml_net_tabs_spec；它们验证当前查询、传输、任务、公共参数/权限/结果和标签生命周期的离线契约。SDK 行为及 Schema/OpenAPI 还有独立校验。TC01–TC20 是完整验收组，不能以这些局部测试将整组或整项迁移宣告通过。当前状态见 [接口规范化计划](../specs/net/NET_API_STANDARDIZATION_PLAN_ZH.md)。

## 1. 当前已有验证覆盖

`dev` 的 net_client_spec 已登记，并涵盖列表解析、部分 Tag/日期、长度完成判断、Referer 编码、上传扫描/去重/自然排序、响应分类/Retry-After、ZIP 文件等离线逻辑。v1 中对排序的断言在 dev 版没有保留。两版该 Spec 都没有对真实查询、流式网络/取消、登录 Cookie、上传 worker 批次和在线预览文档链做端到端断言。

worker 编进库或 Spec 只能说明编译覆盖，不证明行为可用。保持 net_client_spec 并补齐行为验收，不通过复制源码声明字符串来替代运行契约。

相关现有守卫（目标名已按调查提交核对）：

- application_services_spec、chart_workspace_spec、runtime_context_boundary_spec：装配、文档和状态边界。
- qml_ui_backend_surface_spec：QML/Session 直接访问边界。
- extension_product_boundary_spec：旧扩展宿主仍为归档边界。
- dependency_allowlist_spec：网络及新增 HTTP Server 依赖的声明。
- web_module_boundary_spec、android_module_boundary_spec：模块可链接与代表路径，不能替代浏览器/移动端 Net 验收。

建议按真实契约新增测试组：查询/provider、流式传输和任务、上传协议/会话、在线预览文档来源、应用 API/SDK 一致性、网关授权。新 Spec 名称和注册项在实施阶段确定，不在本说明中冒充已存在的测试目标。

## 2. 20 组验收

| 验收 ID | 场景 | 可验证结果 | 对应功能 |
| --- | --- | --- | --- |
| TC01 | 查询/解析与 AND | 离线 provider fixture：上传者、Tag、曲名分别/组合；大小写、中文、多 Tag 字段、重复 ID、错误时间、空数组、无输入、HTML/畸形 JSON。断言最终列表每行满足所有条件，错误不伪装为空结果，completeness 不虚报。 | F02、F03、F04、F05、F06 |
| TC02 | 日期与时区 | 同一 UTC 时间在 Asia/Shanghai 和含 DST 的时区，首日午夜/末日末尾/下一日午夜、反向范围、单边或非法日期；UI 与 HTTP 结果一致。 | F07 |
| TC03 | 缓存与查询快照 | 改变非候选条件复用缓存但重新 AND 过滤；5 分钟边界、forceRefresh、provider/大小写隔离、缓存后的失败；结果分页无重复/遗漏、排序稳定、过期 cursor 明确报错。 | F08 |
| TC04 | 排序与稳定身份 | 六种旧 UI 排序；13、13+、无效等级和平局；多语言状态显示；排序/刷新/追加后勾选仍绑定正确 ID，进行中的 worker 事件更新正确 item。 | F09、F10 |
| TC05 | 连接探测 | 假时钟/假 transport：正常、超过 1000 ms、8 秒 deadline、取消、HTTP 403/429、HTML、JSON 非数组、TLS/网络失败；UI 主线程不出现同步等待，返回分类和耗时。 | F11 |
| TC06 | 流式文件与授权根 | 短写、写入错误、已存在目录、长度不足/未知/声明为零的 chart、媒体空响应、分块/压缩响应、取消/磁盘不足；校验字节层与 manifest；失败不替换有效文件、越界/符号链接/保留文件名拒绝。 | F12、F13、F16 |
| TC07 | 可选视频和 ZIP | includeVideo true/false；404 absent、其他 video 失败、成功 PV 入默认 ZIP、legacy_triplet 三文件模式；重名 ZIP、取消打包、最终 ZIP 可解包且内容与 manifest 一致。 | F14、F19 |
| TC08 | 任务/重试/取消与生命周期 | fake transport + fake clock 覆盖重试预算、800 ms/1000 ms 间隔、资源 60 秒、账号 90 秒、取消在排队/传输/等待中、关闭/销毁 owner、旧事件/generation、resume 条件；Net 与导出进度 token 不互相取消。 | F15、F17、F18、F20、F21、F35 |
| TC09 | 预览缓存 | 同 id/hash 命中、完整 hash 冲突、缺少版本的 TTL、修改/丢失/损坏资源、仅补 PV、404 absent 再次预览不请求；播放期间缓存不能回收，释放与新会话引用正确。 | F22、F23 |
| TC10 | 文档打开与来源策略 | 本地 dirty 文档保存/放弃/取消；打开前、等待期间、回调后切换文档/修改 revision；合法保存决策可继续，未经批准的新状态拒绝；覆盖普通打开和工作区同步两路径的最近记录、会话、自动保存、崩溃恢复、编辑、Save As、关闭及媒体释放。 | F22、F24、F25 |
| TC11 | 扫描与上传队列 | 根目录及一层子目录、深层不扫、自然顺序、MAIDATA.TXT、JPEG/PNG 优先、PV/bg 优先、MP3 限制、缺文件、符号链接、平台路径大小写；多行排序/删除/去重，运行后计划冻结、素材变化不能静默上传。 | F27、F28、F29、F30 |
| TC12 | 登录/上传实际协议 | 本地模拟服务器核验 multipart 字段、UTF-8→MD5 hex、rememberMe=false、重复 formfiles 与规范文件名、文件流、Cookie 关联、账号隔离和 logout；HTTP 200 应用错误及 2xx HTML 不算成功。仅用假账号/素材。 | F33、F34、F35、F36 |
| TC13 | 限流与批次停止 | 429/1015、Retry-After 秒数/HTTP 日期/无效/过期/超大值、登录和上传各仅一次明确重试、第二次停止、5 秒跨任务共享节流；401/403/挑战停批，413/422 单项失败；倒计时可立即取消，不能提前绕过等待。 | F36、F37、F38、F39 |
| TC14 | 结果不明、幂等与失败重试 | 成功/失败/未尝试/未知混合；202 不算完成；已成功项不重发，未知项不自动重试；相同幂等键复用、不同参数 409、父子任务追溯、重启账本恢复；job cancel 不声称撤销上游提交。 | F40、F41 |
| TC15 | 凭据迁移与诊断 | remember 默认关闭、系统凭据成功/不可用、旧明文迁移成功后删除/失败保留、账号列表和偏好脱敏；密码/MD5/Cookie/token/代理密码及登录体不进日志/事件/SDK；日志初始化失败不发送、运行中写入失败阻断、轮转、受控诊断读取。 | F31、F32、F42 |
| TC16 | 网关、浏览器与越权 | 绑定地址/Host、临时配对窗口、准确 Origin/预检、批准/拒绝/过期/撤销 token、权限与所有权、跨客户端 job/account/artifact、null Origin、URL token、DNS rebinding、请求/连接/速率上限；同源页面和外部 HTTPS 页面实测 CORS/本地网络权限。 | F46、F47 |
| TC17 | 注册表与各调用面一致性 | 集合比较 operation registry、OpenAPI、HTTP、CLI、SDK、说明；秘密字段的 writeOnly/日志策略、DTO 默认值、未知字段、64 位字符串、分页与错误；新增能力未实现时 available=false。同一 fixture 经 QML/HTTP/CLI 得到同样业务结果。 | F44、F50 |
| TC18 | 通用网络与代理 | GET-only 与 timeout/文本上限、二进制流式下载、文件授权；http/https、公网/loopback/private 目标授权、重定向/DNS/连接复核；代理 profile/凭据脱敏与权限；配置 Net 代理不会改变 update 或其他网络域。 | F44、F45、F46、F47、F48、F49、F50 |
| TC19 | 平台与发布 | 每个宣称支持的平台：Release 包 TLS 后端、HTTPS、代理、中文/空格路径、凭据库、cache/ZIP/临时资源和网关依赖；WASM 单独验证 CORS/异步/存储，移动端单独验证 sandbox/后台行为，记录明确 capability。 | 跨能力验收 |
| TC20 | 产品全链路 | QML 菜单与单实例页面、查询/日期/排序/勾选/目录记忆、下载/PV/ZIP、上传扫描/排序/失败重试/账号记忆、预览编辑/Save As/切回工程、取消/关闭、三语言/主题、错误以纯文本显示；逐项核对 50 项，保留验收证据。 | F01、F10、F26、F30、F43 |

TC19、TC20 为跨平台/产品验收，虽然没有逐条写入每个 feature 的 test_ids，仍适用于全部相关功能。TC16、TC17 同时覆盖新增接口，不限于旧功能。

离线用例优先使用可注入 transport/clock/storage/credential 端口及本地模拟服务器。断言最终文件、稳定状态、实际请求、任务身份和可观测结果。不要使用真实 Majdata 的账号/上传来实现每次 CTest 的通过条件，也不要用长时间 sleep 模拟 Retry-After。

需要真实服务验证时，在明确获准的测试账号和专用素材上核实上游响应/配额/认证，记录验证日期与 provider 版本；预生产测试无法证明日后第三方服务永远兼容。账号凭据和完整本机路径不写入公开报告。

产品验收由测试人员操作；agent 若需界面自动化，遵守所在工作区 AGENTS 的明确授权要求。现有构建/测试/API/文件工具能完成的验证优先使用这些工具。

## 3. 可独立交付的任务包

| 任务 | 输入与范围 | 主要改动位置 | 交付门槛 |
| --- | --- | --- | --- |
| P0 契约冻结 | 50 项及来源差异；完整 DTO、状态、错误/秘密字段、34 个拟定操作和平台条件 | docs + 拟定能力注册表/schema/客户端类型；不重写旧归档 SDK | 能力/请求/结果都有 owner；清单与 schema 可机读；接口均明确 proposed/available=false |
| P1 引擎与应用服务 | F02–F08、F11、F13、F15–F18、F21、F35、F41 | media_tools/net、app/services/net、ApplicationServices 装配、CMake 注册 | TC01–TC06、TC08；native 无 UI 消费可运行；Qt 线程/库边界守卫通过 |
| P2 查询下载 UI | F01、F09、F10、F12、F14、F19、F20、F43 | app/ui/net、ApplicationContext、菜单、偏好/翻译资源；PV/ZIP 引擎补齐 | TC04、TC06、TC07；TC20 的查询/下载部分；页面生命周期与取消正确 |
| P3 上传账号 | F26–F34、F36–F40、F42 | 扫描器/上传 provider、账号/凭据端口、任务重试、上传 UI、日志 | TC11–TC15；素材与已成功项不重传，敏感字段不会泄漏，旧凭据迁移有事务结果 |
| P4 在线预览 | F22–F25 | 缓存 manifest、准备/打开用例、ChartWorkspace 会话来源、DocumentSessionHost 文件/自动保存流程 | TC09、TC10；普通打开/工作区同步两路径都尊重 origin，现有播放/文档 Spec 仍通过 |
| P5 公开调用 | F44–F50 及全部业务的 API 适配 | app/services/api、HTTP/CLI adapter、新 SDK；通用网络/代理；依赖 allowlist/打包 | TC16–TC18；先完成同源示例，再外部网页；34 个操作逐项 capability，代理域彼此隔离 |
| P6 发布结案 | 全部功能与支持平台 | 平台包、验收报告、说明/清单/运行时 capability | TC19、TC20；每项明确通过/不支持/阻塞原因；只有已支持平台进入完整迁移声明 |

P1 也包括恢复 probe 和稳定查询最终过滤；仅把 Dialog 搬成 QML 不满足任务。ApplicationServices 的生产触网组件通过 typed slot 安装，离线装配规格不因此引入 QtNetwork；JobRegistry 在拟定的 app/services/jobs 中供各领域共用。P3 同时处理旧记忆密码与上传日志，不把它们留到开放 API 之后补。P5 要检查现有 NetworkUpdateFetcher 的系统代理安装方式，收敛到请求域端口，保持更新检查的独立超时和小响应限制。

## 4. 下游 agent 执行顺序

1. 阅读目标分支当前 AGENTS、开发指南、本清单及 API 规范，核对 source commit；确认这些 proposed 路径/服务尚未被其他变更创建。
2. 用 Git 比较目标当前 head 与调查基线的相关 owner、接口、CMake、文档守卫；有漂移先更新 feature/evidence，而非盲目按旧行号改文件。
3. 为本任务选择明确的 F/TC 集合，检查前置阶段是否交付；共享 DTO/schema 改动先明确 owner 和兼容影响。
4. 先建立服务用例和假端口验收，再接 UI 或 HTTP；新翻译与偏好经目标分支现有资源系统；修改 CMake 的所属库与 Spec 清单。
5. 按变动范围执行 Release、必要 Spec、模块/文档守卫和受影响产品验收；记录真实输出、失败与未执行项。
6. 审阅完整差异、格式和接口漂移；更新功能状态和证据，提供下游可核验的报告；达到门槛才标完成。commit/push 前遵守工作区要求，先代码审阅与必要构建/测试。

建议下游任务请求使用如下格式：

```text
目标：在 dev 完成 P<阶段>；覆盖 F<集合>，验收 TC<集合>。
基线：当前 dev commit；核对与 cb52f6e8103e2773e839b797cdbd2522e780e2c7 的 owner 差异。
输入：迁移评估、API 契约、net-migration-inventory.json、已交付前置任务。
约束：所属模块、单工作区、typed ports、异步身份、Release/build-devtools、凭据/文件/网络授权。
交付：代码、契约/schema/SDK/翻译/文档同步、实际验证、残余条件、50 项中所选功能的结案状态。
```

## 5. 构建与检查约定

本工作区使用 build-devtools 和 Release，构建统一通过本机 AGENTS 指定的 MiaCode Release 脚本。脚本失败时先核对路径、参数、进程处理和退出码，再判断产品代码。启用 MIACODE_BUILD_DEV_TOOLS=ON 后，以 CTest 的实际清单选择已存在的目标；此文档不授权绕过本机并发/构建规则。

实施后的检查示例：

```text
python scripts/governance/module_layering.py
python scripts/governance/docs_index.py --sync
python scripts/governance/docs_index.py --check
ctest --test-dir build-devtools -C Release -N
ctest --test-dir build-devtools -C Release -R "^net_client_spec$" --output-on-failure
```

先运行被改契约对应的 Spec，再按新失败/风险扩大范围；P4 必须包含受影响文档/播放守卫，P5 包含依赖/扩展归档边界，新 API 不能偶然恢复旧宿主。新行为用例的名称确定后加入 CMake registry 并记录实际命令。平台包/TLS、真正浏览器和用户交互验收不能只用 source-contract Spec 替代。

## 6. 最终完成声明需要的材料

- 50 项矩阵的每项状态、实现 owner、实际验收 ID、测试输出/手工证据和行为调整；“底层保留”不是终态。
- 34 个拟定操作最终的实现/平台可用性、注册表/OpenAPI/SDK 版本与能运行的网页示例；有删改需同步本文。
- 上游兼容 fixture、真实服务核验日期、客户端错误/限流策略及公开结果，不带秘密或内部路径。
- 明文凭据、ZIP/PV、缓存无视频、零长度 chart、上传未知结果、代理作用域等行为调整的结案记录。
- 窗口关闭、账号撤销、进程崩溃/重启、缓存释放和旧异步回调的资源/数据完整性证据。
- 支持平台明确列出；未支持/未核验的平台与接口在 capability 中保持不可用及具体原因。

## 桌面本阶段验证口径（2026-10-10）

目标及实现记录见 [迁移目标](../specs/net/NET_MIGRATION_TARGET_ZH.md) 与 [接口规范化计划](../specs/net/NET_API_STANDARDIZATION_PLAN_ZH.md)。账户、扫描和上传由用户执行运行验收，开发验证采用源码、接口结构和 Release 编译。自动运行采用下载、预览资源、文档文件、任务及翻译的定向 Spec；本地固定样本输出在 `build-devtools` 下创建并随测试清理。
