---
lifecycle: working
owner: src
last_verified: 2026-10-09
---

# Net 功能迁移评估与交接说明

本文面向在 `dev` 上实施迁移的程序员和 agent。它是依据固定提交编写的调查结果与实施计划；新服务、新接口、QML 页面和验收用例均属于待实施内容。公共接口的建议契约见 [Net 与通用网络 API 规范](NET_API_CONTRACT_ZH.md)，验收与任务交付要求见 [迁移验收清单](../../tests/NET_MIGRATION_TEST_CHECKLIST_ZH.md)。可机读清单见 [net-migration-inventory.json](net-migration-inventory.json)。

桌面工作区接入账户、上传、下载、在线预览、接口目录与工具箱标签页。以下固定提交、功能状态和调查说明保留原调查语境；本阶段目标见 [迁移目标](NET_MIGRATION_TARGET_ZH.md)，当前进度以 [接口规范化计划](NET_API_STANDARDIZATION_PLAN_ZH.md) 与实际源码为准。新增功能页面使用现有标签页。

## 1. 结论与适用范围

**桌面端 Majdata Net 的全部业务能力具备完整迁移条件。** `dev` 已保留网络引擎，主要缺口是应用服务、QML 产品入口、旧版后续增加的查询/下载能力，以及在线预览的文档持久化策略。迁移必须遵循 `dev` 的服务和端口边界，逐项验收后才能宣布恢复功能。

**把整个旧 net 接口面原样搬回去不成立。** v1 的扩展宿主也提供通用 HTTP GET/下载，以及本机、内网、代理处理器；`dev` 明确移除了该宿主。其可用行为可以由新应用 API 和通用网络服务承接，但旧的无目标隔离、任意目标路径写入、进程全局代理、明文密码存储等执行方式需要改变。重建这些能力不要求恢复整个 v1 扩展系统。

网页调用桌面 MiaCode、独立浏览器/WASM 版本、原生移动端是三种不同部署形态。前者可通过新增本机网关调用完整桌面业务；后两者需要网络、文件和凭据的专用适配器及平台验证，不能从桌面源码的可移植性推出已经完整支持。

本文将“net 全部功能”分为：

- Majdata Net 查询、批量下载、批量上传、账号、在线预览及关联状态/文件语义。
- v1 `miacode.net` 的通用网络 facade、宿主处理器和权限契约。
- `dev` 的更新检查仅用于说明网络依赖与代理影响；它是已存在的独立领域服务。PV 压缩和本地 ZIP 工具是可复用邻接能力，不把它们的产品参数认作 Majdata 服务端限制。

## 2. 调查基线和证据强度

| 项目 | 固定提交 | 调查方式 |
| --- | --- | --- |
| 来源 `origin/MiaCode_v1` | `6573efb611056bccd046dd17bc2f4cceda78bf88` | 读取 net 全部实现、扩展网络处理器，以及菜单、文档打开、自动保存、SDK、CMake 和 Spec 调用链 |
| 目标 `origin/dev` | `cb52f6e8103e2773e839b797cdbd2522e780e2c7` | 读取保留的网络实现、静态库定义、服务/运行时端口、QML 入口、文档权威、依赖守卫和扩展归档守卫 |
| 本地阅读分支 `MiaCode_v1` | `db5d120ffe800b0e47093aabb7b435b319896afb` | 对读取的 net、扩展及相关文档/菜单路径核对来源差异；net 实现与来源基线一致 |

远端分支已于 2026-10-09 通过 Git fetch 核对。以下结论来自 Git 对象中的代码与构建清单，不由目录名或过期注释推断。

本次仅调查并编写文档，未运行两版软件，未进行真实账号登录或上传，未验证 Majdata 服务当前返回值，也未构建 `dev`。因此“保留”表示源码存在且列入构建，“可迁移”表示架构可承接，均不是功能实测通过。

注意：`dev` 的 `cmake/devtools/specs/core.cmake` 仍称 Net 只在 Spec 编译、产品没有 QtNetwork。现行 `MiaCodeModules.cmake` 已将 net 列入 `miacode_media_tools`，根 CMake 也链接 QtNetwork，更新服务已使用网络。该注释已过期，迁移时应同步修正文档和依赖 allowlist。[D01](#evidence-d01)、[D16](#evidence-d16)、[D17](#evidence-d17)、[D18](#evidence-d18)

## 3. 50 项功能与接口清单

状态说明：“底层保留”仍缺 Net 产品调用入口；“部分保留”存在链路缺口；“仅旧处理器”只证明宿主有处理分支，不代表普通 SDK 对它提供了可调用方法。每项对应的验收 ID 保存在机读清单。

| ID | v1 已有行为或处理器 | dev 状态 | 迁移动作／阶段 | 代码证据 |
| --- | --- | --- | --- | --- |
| F01 | 下载工具菜单、独立窗口复用和激活 | 缺失 | 改为 QML Net 页面与菜单入口（P2） | [V18](#evidence-v18)、[V19](#evidence-v19)、[D20](#evidence-d20) |
| F02 | 按上传者查询；空结果时尝试大小写形式和普通文本回退 | 底层保留 | 保留查询策略，返回最终筛选结果（P1） | [V01](#evidence-v01)、[D02](#evidence-d02) |
| F03 | Tag 查询兼容 tag: 前缀、普通 Tag 文本及大小写回退 | 底层保留 | 把候选查询与本地 Tag 过滤集中到引擎（P1） | [V01](#evidence-v01)、[V02](#evidence-v02)、[D02](#evidence-d02) |
| F04 | 按曲名搜索及大小写回退 | 底层保留 | 定义曲名 contains 与大小写规则（P1） | [V01](#evidence-v01)、[V02](#evidence-v02)、[D02](#evidence-d02) |
| F05 | 上传者、Tag、曲名、日期同时填写时执行 AND 过滤 | 部分保留 | 搬出 Dialog 的最终过滤；纠正 dev 候选合并语义（P1） | [V01](#evidence-v01)、[V02](#evidence-v02)、[D02](#evidence-d02) |
| F06 | 解析 id/title/artist/designer/uploader/hash/levels/UTC 时间及多个 Tag 字段 | 底层保留 | 保持字段兼容；报告跳过行及完整性（P1） | [V03](#evidence-v03)、[D02](#evidence-d02) |
| F07 | 本地日期首尾日包含、反向日期自动归一化 | 部分保留 | 服务接收明确 timeZone；UI 保留首尾日语义（P1） | [V04](#evidence-v04)、[V02](#evidence-v02)、[D02](#evidence-d02) |
| F08 | 候选缓存 5 分钟；无缓存时上传者、Tag、曲名按序选取一个来源 | 缺失 | 迁移缓存与失效；与最终结果分页分开（P1） | [V01](#evidence-v01)、[D03](#evidence-d03) |
| F09 | 等级、上传时间、显示状态各正反排序；等级取最高值且 + 加 0.5 | 缺失 | 恢复 UI 排序；公共接口按稳定枚举排序（P2） | [V05](#evidence-v05)、[D03](#evidence-d03) |
| F10 | 结果表、勾选、全选、取消选择、按 chartId 映射状态 | 缺失 | 使用列表模型和稳定 itemId，保持用户选择（P2） | [V07](#evidence-v07)、[V02](#evidence-v02)、[D20](#evidence-d20) |
| F11 | 8 秒实际 API 探测；正常/慢/超时/失败/阻断分类及取消 | 缺失 | 异步探测；慢阈值保留 1000 ms（P1） | [V06](#evidence-v06)、[V07](#evidence-v07)、[D03](#evidence-d03) |
| F12 | 输出目录记忆；首次回退桌面或用户主目录 | 缺失 | 迁移非敏感偏好；网页使用授权目录引用（P2） | [V07](#evidence-v07)、[D20](#evidence-d20) |
| F13 | 下载 track→track.mp3、完整 image→bg.jpg、chart→maidata.txt | 底层保留 | 用固定资源枚举与 provider 映射（P1） | [V08](#evidence-v08)、[V09](#evidence-v09)、[D04](#evidence-d04) |
| F14 | 默认下载可选 video→pv.mp4；404 视为无 PV | 缺失 | 恢复可选资源；持久记录 absent 状态（P2） | [V08](#evidence-v08)、[D04](#evidence-d04) |
| F15 | 后台 QThread，逐谱面、逐资源串行执行 | 底层保留 | 服务拥有任务；保留默认单并发策略（P1） | [V08](#evidence-v08)、[D04](#evidence-d04) |
| F16 | 流式写入 QSaveFile、长度验证；声明长度为零的 maidata 特例 | 底层保留 | 检查每次 write；统一零长度与整目录提交策略（P1） | [V09](#evidence-v09)、[D02](#evidence-d02) |
| F17 | 资源最多尝试 3 次、间隔 800 ms；列表 GET 断连另有 1 次重试 | 底层保留 | 统一预算，避免重试层相乘（P1） | [V08](#evidence-v08)、[V09](#evidence-v09)、[D04](#evidence-d04) |
| F18 | HTTP 403/429 或挑战 HTML 暂停整队列 | 底层保留 | 暴露 blocked 状态及明确恢复条件（P1） | [V08](#evidence-v08)、[D04](#evidence-d04) |
| F19 | 下载后保留目录并可另建 ZIP；名称净化、重名追加序号 | 底层保留 | 恢复入口；修正 ZIP 仅打包三文件而遗漏 PV（P2） | [V10](#evidence-v10)、[V08](#evidence-v08)、[D02](#evidence-d02)、[D04](#evidence-d04) |
| F20 | 逐行状态、完成计数、字节数、速度及最慢资源诊断、日志显示 | 部分保留 | 结构化状态和事件；UI 负责显示翻译（P2） | [V08](#evidence-v08)、[D04](#evidence-d04) |
| F21 | 下载/探测取消；忙时关闭先请求取消并等待完成 | 部分保留 | 取消句柄归任务 owner；关闭不销毁运行中的 worker（P1） | [V07](#evidence-v07)、[V08](#evidence-v08)、[D04](#evidence-d04) |
| F22 | 按行准备临时资源并打开谱面；进程临时目录按 id/hash 区分 | 缺失 | 准备与打开拆成两个命令；缓存独立于 UI 生命周期（P4） | [V11](#evidence-v11)、[V12](#evidence-v12)、[D10](#evidence-d10) |
| F23 | 完整缓存复用；勾选 PV 后仅补下载缺失资源 | 缺失 | 缓存 manifest 校验；无 PV 避免反复请求（P4） | [V11](#evidence-v11)、[V08](#evidence-v08)、[D04](#evidence-d04) |
| F24 | 打开前保存/放弃/取消保护；加载到现有文档和预览链 | 缺失 | 沿 DocumentBridge、ChartWorkspace 和 DocumentSessionHost 接入（P4） | [V12](#evidence-v12)、[D09](#evidence-d09)、[D12](#evidence-d12) |
| F25 | 临时文档跳过自动保存/崩溃恢复/最近文件，保持原最近目录及会话路径 | 缺失 | 新增文档来源/持久化策略；覆盖全部文件路径（P4） | [V12](#evidence-v12)、[V13](#evidence-v13)、[D10](#evidence-d10)、[D11](#evidence-d11) |
| F26 | net.batchUpload.open 内部命令打开独立上传窗口 | 缺失 | 增加 QML 上传入口及明确业务 API；兼容入口单独登记（P3） | [V20](#evidence-v20)、[V18](#evidence-v18)、[D15](#evidence-d15) |
| F27 | 扫描根目录及一层子目录，按自然数字顺序排序 | 底层保留 | 复用扫描器；增加授权根和结构化拒绝原因（P3） | [V14](#evidence-v14)、[D06](#evidence-d06) |
| F28 | 忽略素材文件名大小写；必需 maidata、JPEG/PNG、MP3，可选 pv.mp4/bg.mp4 | 底层保留 | 保持优先顺序；上传前校验实际素材与快照（P3） | [V14](#evidence-v14)、[D06](#evidence-d06) |
| F29 | 追加扫描结果并按目录去重 | 底层保留 | 去重按平台路径规则及实际目录身份（P3） | [V14](#evidence-v14)、[D06](#evidence-d06) |
| F30 | 拖动多行排序、删除选中、清空队列，按当前顺序上传全部行 | 缺失 | 队列顺序由稳定 itemId 管理；运行中锁定计划（P3） | [V15](#evidence-v15)、[D20](#evidence-d20) |
| F31 | 账号/密码输入与必填检查；不记忆时清空密码框，批次后清理会话值 | 缺失 | 独立账号会话服务与凭据句柄（P3） | [V15](#evidence-v15)、[D05](#evidence-d05) |
| F32 | 可记忆账号/密码；旧版直接写 preferences.json 的 app.net_upload_* | 缺失 | 改用系统凭据存储并迁移清理旧明文（P3） | [V15](#evidence-v15)、[D08](#evidence-d08) |
| F33 | multipart 登录；UTF-8 密码 MD5 hex、rememberMe=false；同 manager Cookie 用于上传 | 底层保留 | MD5 限于 provider 兼容；隔离账号 Cookie（P3） | [V16](#evidence-v16)、[D05](#evidence-d05) |
| F34 | 重复 formfiles 字段，规范上传文件名；可选 PV；文件以 QIODevice 发送 | 底层保留 | 复用协议；锁定文件快照和 MIME/大小检查（P3） | [V16](#evidence-v16)、[D05](#evidence-d05) |
| F35 | 登录/上传 90 秒超时，100 ms 取消轮询 | 底层保留 | 改为可取消异步 transport；统一超时结果（P1） | [V16](#evidence-v16)、[D05](#evidence-d05) |
| F36 | 登录要求 HTTP 200，上传要求 2xx；拒绝应用失败字段和挑战/限流 | 底层保留 | 保留分类器；增加预期响应格式验证（P3） | [V16](#evidence-v16)、[V17](#evidence-v17)、[D07](#evidence-d07) |
| F37 | 429/Cloudflare 1015 等待 Retry-After，缺省 60 秒；登录/上传各重试 1 次 | 底层保留 | 保留秒数/HTTP 日期解析；大等待转为用户恢复（P3） | [V16](#evidence-v16)、[V17](#evidence-v17)、[D05](#evidence-d05)、[D07](#evidence-d07) |
| F38 | 可继续的行完成后距下一行等待 5 秒并显示倒计时 | 底层保留 | provider 调度器共享节流；失败但未停批也保持间隔（P3） | [V16](#evidence-v16)、[D05](#evidence-d05) |
| F39 | 401/403/挑战/二次限流停批；413/422 通常只使当前行失败 | 底层保留 | 稳定错误代码、停止原因与 pending 项（P3） | [V17](#evidence-v17)、[V16](#evidence-v16)、[D07](#evidence-d07) |
| F40 | 显示失败详情后重试失败行；停批时包含未尝试行，排除已成功行 | 缺失 | 新 job 引用父 job，禁止自动重复结果不明的上传（P3） | [V15](#evidence-v15)、[D05](#evidence-d05) |
| F41 | rowStatus/rowOutcome/failureDetail/progress/summary/finished 信号与最终计数 | 部分保留 | 结构化 item 结果和任务终态；API 可查询（P1） | [V16](#evidence-v16)、[D05](#evidence-d05) |
| F42 | 独立 net-upload.log 追加并 flush；日志打开失败即停批，UI 暴露路径/详情 | 缺失 | 接入 DebugLog，脱敏、轮转及受控诊断读取（P3） | [V16](#evidence-v16)、[D05](#evidence-d05) |
| F43 | 中/英/日界面、主题与状态文案 | 契约改变 | 使用 dev 的 qsTrId/qtTrId、翻译资源与共享 QML 控件（P2） | [V07](#evidence-v07)、[V15](#evidence-v15)、[D19](#evidence-d19) |
| F44 | miacode.net.fetch(url, options)：http/https GET，返回 status/text；仅 timeoutMs 生效 | 宿主移除 | 新增 network.http.fetch，声明 GET、上限与目标授权（P5） | [V21](#evidence-v21)、[V22](#evidence-v22)、[D15](#evidence-d15) |
| F45 | miacode.net.download(url, targetPath)：GET 后整包写文件，返回 status/path | 宿主移除 | 新增流式 network.http.download，使用文件授权引用（P5） | [V21](#evidence-v21)、[V22](#evidence-v22)、[D15](#evidence-d15) |
| F46 | net/fetchLocalhostWithoutPrompt 处理器；没有独立普通 SDK wrapper | 仅旧处理器／宿主移除 | 映射 loopback 目标授权；不以别名代替授权校验（P5） | [V21](#evidence-v21)、[V23](#evidence-v23)、[D15](#evidence-d15) |
| F47 | net/fetchPrivateNetworkWithoutPrompt 处理器；与普通 GET 共用实现 | 仅旧处理器／宿主移除 | 映射 private-network 目标授权，核验 DNS 与重定向（P5） | [V21](#evidence-v21)、[V23](#evidence-v23)、[D15](#evidence-d15) |
| F48 | net/getProxySettings 处理器读取应用代理 type/host/port/user | 仅旧处理器／宿主移除 | network.proxy.get 返回请求域代理配置并脱敏（P5） | [V21](#evidence-v21)、[D17](#evidence-d17) |
| F49 | net/setProxySettings 处理器设置含密码的进程全局 QNetworkProxy | 仅旧处理器／宿主移除 | network.proxy.set 使用请求域 profile 与凭据引用（P5） | [V21](#evidence-v21)、[D17](#evidence-d17) |
| F50 | network.fetch/network.unsafe 权限和 JS facade；net 类型/注册清单存在缺口 | 宿主移除 | 统一能力注册表、权限校验、SDK 和跨平台可用性（P0） | [V22](#evidence-v22)、[V23](#evidence-v23)、[V24](#evidence-v24)、[D15](#evidence-d15) |

## 4. 旧版实际外部协议

Majdata 的基地址在客户端和上传 worker 中分别写死为 `https://majdata.net/api3/api`。下表是客户端当前使用方式，属于兼容适配器的输入证据，不是服务端承诺，也不是 MiaCode 新公共 API 的路径设计。[V01](#evidence-v01)、[V09](#evidence-v09)、[V16](#evidence-v16)

| 操作 | 方法与上游路径 | 当前参数或映射 |
| --- | --- | --- |
| 列表查询 | GET `/maichart/list` | `sort=`、`search=`；上传者为 `uploader:<值>`，Tag 为 `tag:<值>`，曲名为文本 |
| 连接探测 | 同列表接口 | `search=__miacode_connection_probe__`；要求成功 JSON 数组 |
| 音频 | GET `/maichart/{id}/track` | 写为 `track.mp3` |
| 背景图 | GET `/maichart/{id}/image?fullImage=true` | 写为 `bg.jpg` |
| 谱面 | GET `/maichart/{id}/chart` | 写为 `maidata.txt` |
| 视频 | GET `/maichart/{id}/video` | 写为 `pv.mp4`；v1 将 404 视为可选缺失 |
| 登录 | POST `/account/Login` | multipart：`username`、UTF-8 密码 MD5 的 hex、`rememberMe=false` |
| 上传 | POST `/maichart/upload` | 重复 `formfiles` 字段；文件名为 maidata、bg 对应扩展名、track.mp3、可选 pv.mp4 |

下载与上传各自设置 User-Agent、Accept 和网页 Referer，登录与上传通过同一个 manager 的 Cookie 状态关联。浏览器前端应调用 MiaCode 的规范接口，provider 负责兼容这些上游细节。

旧通用网络 GET 与 Majdata 客户端是两套实现。`miacode.net.fetch` 接受 options，但实现只读取 timeoutMs（默认 15 秒，约束 1–60 秒）；它没有实现浏览器 fetch 的 method、headers、body、流或 AbortSignal 语义。`download` 先整包读取再用 QFile 覆盖目标路径。四个 GET 处理分支没有独立目标网段校验，名称中的 Localhost/PrivateNetwork 不形成隔离。代理设置调用 `QNetworkProxy::setApplicationProxy`，会影响其他使用全局代理的网络路径。[V21](#evidence-v21)、[V22](#evidence-v22)、[V23](#evidence-v23)

已核对的旧业务没有图表删除/编辑、账号注册、评论/收藏等服务端管理接口。新增这些业务应另立 provider 能力和契约，不能从“net 全部迁移”推断它们已存在。

## 5. dev 的承接位置

`dev` 产品入口是 QML Bootstrap 与 QObject `runtime::Session`，文档权威为 `ChartWorkspace`。产品没有旧 MainWindow，也没有 ExtensionManager、EmbeddedExtensionRuntime、Open Bridge 宿主。下层库不得依赖 app，services/runtime 不得包含 ui。[D08](#evidence-d08)、[D09](#evidence-d09)、[D15](#evidence-d15)、[D19](#evidence-d19)

| 待实施职责 | 建议位置 | 现有可复用入口与限制 |
| --- | --- | --- |
| provider、DTO、查询规则、文件传输与素材打包 | `src/media_tools/net/` | 属于 `miacode_media_tools`；复用现有 scanner、diagnostics、ZIP；返回值类型与事件，避免界面依赖 |
| NetService、账号会话与 provider 端口 | `src/app/services/net/` | 由 ApplicationServices 的 typed slots/非触网装配承接；生产 adapter 由 Bootstrap/runtime 创建安装；新位置/类名均为方案 |
| 任务登记、调度与公共能力分发 | `src/app/services/jobs/`、`src/app/services/api/` | 公共 JobRegistry 服务 Net 和通用网络，后续领域按同一任务契约接入；现有 JobProgressService 只负责投影 |
| 文档来源与临时持久化策略 | `ChartWorkspace` 的会话元数据 + `runtime/document/` | 使用 DocumentBridge 的离开文档 continuation，接到现有单工作区；不要在 HTTP 路由中直接写文档 |
| 查询、上传队列、任务模型和页面 | `src/app/ui/net/` | 按 ApplicationContext 的领域入口模式；用 QAbstractListModel 与 QML 共享控件 |
| 网络线程及文件资源生命周期 | transport/任务 owner | manager、reply、QIODevice 在所属线程创建/操作/销毁；旧 worker 只可作为原生平台过渡实现 |
| 网页 HTTP 与 CLI 适配器 | `src/app/api/`、独立 CLI 入口 | 薄协议转换，调用同一分发器；不复制业务逻辑，不提供任意 QObject/raw 方法执行 |

`JobProgressService` 只能呈现一个活动任务，`begin()` 会替换展示。它不是多任务登记表。新增任务服务持有 job/item 状态，进度服务只投影选中的任务并核验 token，防止 Net 与视频导出互相覆盖或取消。[D13](#evidence-d13)

ApplicationServices 的离线装配规格仅链接 Qt Core/Gui。参照现有 UpdateFetcher/UpdateService 的安装槽位，服务与公开 DTO 不把 QNetworkAccessManager 或 NetClient 的触网实现带入该闭包；生产 transport/provider、系统凭据、HTTP adapter 在 Bootstrap/runtime 一侧构造后安装。DocumentBridge 已是 services 中的类型端口，其实现属于 DocumentSessionHost，可复用 continuation 而不让服务包含 ui。[D08](#evidence-d08)、[D12](#evidence-d12)

`src/tools/` 在 dev 只放 Spec。不要将业务重新放进这个目录，不要照搬旧 QDialog、UiText 偏好读写或 MainWindow 状态。当前文案来自 Qt Linguist，QML 用 qsTrId，C++ 用 qtTrId；偏好通过 PreferenceDocument/PreferenceProvider 边界访问。[D19](#evidence-d19)

## 6. 实施前必须解决的行为差异

1. **查询结果边界**：v1 客户端返回候选，Dialog 再做 AND 过滤；dev 会合并曲名与其他来源的候选。新 API 必须只输出最终筛选结果。旧接口没有分页/总数证明，不能承诺检索到服务端全部匹配；“完整迁移”指功能覆盖。
2. **密码与调用日志**：v1 把密码存入偏好 JSON，扩展调用诊断还可能记录参数。迁移账号记忆功能时必须改用凭据端口，先完成安全迁移再删除旧键；密码、MD5、Cookie、代理密码和 bearer 不进入请求事件/日志。
3. **临时预览来源**：dev 普通打开会添加最近记录、准备崩溃恢复，自动保存也没有 onlinePreview 判定。只调用普通 openFileAtPath 会改变旧行为。文档来源策略要同时作用于打开、工作区同步、自动保存、历史、恢复、Save/Save As 和关闭。[D10](#evidence-d10)、[D11](#evidence-d11)
4. **缓存完整性**：v1 无 PV 时允许下载成功，但 `requireVideo=true` 的缓存检查仍要求 pv.mp4 存在，下一次预览会重复请求。使用资源 manifest 的 present/absent/failed 状态；hash 是上游不透明版本提示，不作为文件完整性的哈希保证。
5. **ZIP 与零长度**：v1 ZIP 只包含三文件；video 下载成功也不入包。声明为零长度的 maidata 可以在流式下载中成功，但 payload 打包和预览完整性检查又拒绝空文件。新契约统一由 manifest 决定资源有效性，ZIP 包含成功下载的 PV；旧三文件模式只留在明确的兼容选项。
6. **上传结果不明**：取消/超时不能证明服务端没有接受上传。旧失败行重试规则没有处理这种情况。新 item 需标记 outcome_unknown，通过用户核对或 provider 可证实的远端状态决定再次上传，避免自动重发。
7. **通用网络与 SDK**：JS facade 暴露 net.fetch/download，但旧 TypeScript 面未声明 net；能力 descriptor 与 handler 的网络覆盖也不一致。无普通 wrapper 的 localhost/内网/代理处理器必须明确登记可用性，不能因处理分支存在就标为稳定公共 API。
8. **返回值、路径与限流**：旧状态使用翻译文本和行号，单文件原子写不等于整谱面事务，去重一律忽略大小写也不符合所有平台。迁移为稳定 ID/枚举、受授权根约束的规范路径、整任务 manifest 和 provider 级共享节流；拒绝写入错误与越界重定向。

## 7. 迁移阶段与完成标准

| 阶段 | 前置 | 工作与交付 | 完成门槛 |
| --- | --- | --- | --- |
| P0 基线与契约 | 本说明 | 核对固定提交、50 项清单；确定 DTO、状态机、权限、能力注册表和上游离线 fixture；生成 OpenAPI/JSON Schema 与 SDK 草案 | 每项有 owner、实施项与验收 ID；新增 API 标为待实施 |
| P1 引擎与服务 | P0 | 统一异步传输、候选/最终过滤、缓存、probe、文件写入、任务 owner 和结构化事件 | 离线网络/取消/重试/文件失败用例通过；服务可无 QML 工作 |
| P2 查询与下载界面 | P1 | QML 查询表、排序、选择、目录记忆、PV、ZIP 和进度 | 查询到目录/ZIP 全链路可用；主题与翻译齐全 |
| P3 上传与账号 | P1 | 扫描队列、凭据迁移、Cookie 隔离、上传、限流、诊断、失败/未尝试项重试 | 确认已成功项不重发；敏感信息与未知结果用例通过 |
| P4 在线预览 | P1、P2 | 缓存 manifest；准备/打开两阶段；文档来源策略与离开保护 | 不污染历史/自动保存；旧异步结果不能打开新会话中的错误谱面 |
| P5 开放接口 | P0、P1；业务接口需 P2–P4 | 同一能力注册表驱动 HTTP、CLI 与 SDK；配对、目标/文件授权；通用 GET/下载/请求域代理；网页示例 | 真实浏览器可查询、下载、查看任务、取消；越权和 API/SDK 漂移用例通过 |
| P6 平台与交付 | P2–P5 | Windows/macOS/Linux 打包/TLS/中文路径验收；移动/WASM 按平台 capability 声明支持程度 | 50 项逐项结案；变更语义已记录；已支持平台都有证据 |

阶段可以按领域拆成下游任务，但共享 DTO、事件、状态机、权限和工作区契约必须先冻结。没有必要在同一个变更中重构全仓库或恢复整个扩展宿主。

移植文档到 dev 时将三份说明和机读清单一起迁入，更新 dev 文档入口并运行其 `docs_index.py --sync`。实现跨库调整后运行模块分层守卫；只有能力和验收真的落实后，才更新文档 lifecycle 和运行时 capability。

## 8. 源码证据

链接固定到调查提交，避免后续分支移动使结论失去可追溯性。机读清单同时保留 symbol 与实际行号，实施前应重新检查基线漂移。

| 证据 | 基线源码与定位 | 作用 |
| --- | --- | --- |
| <a id="evidence-v01"></a>V01 | [v1：src/tools/net/NetClient.cpp:558](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetClient.cpp#L558)；`NetClient::queryCharts(` | 查询候选、回退与缓存 |
| <a id="evidence-v02"></a>V02 | [v1：src/tools/net/NetBatchDownloadDialog.cpp:738](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetBatchDownloadDialog.cpp#L738)；`void NetBatchDownloadDialog::queryCharts(` | 查询最终过滤与列表状态 |
| <a id="evidence-v03"></a>V03 | [v1：src/tools/net/NetClient.cpp:178](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetClient.cpp#L178)；`QList<NetChartSummary> parseChartListJson(` | 远端列表字段与标签兼容 |
| <a id="evidence-v04"></a>V04 | [v1：src/tools/net/NetClient.cpp:222](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetClient.cpp#L222)；`QList<NetChartSummary> filterChartsByLocalDateRange(` | 本地日期范围 |
| <a id="evidence-v05"></a>V05 | [v1：src/tools/net/NetClient.cpp:282](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetClient.cpp#L282)；`void sortNetDownloadJobs(` | 六种排序 |
| <a id="evidence-v06"></a>V06 | [v1：src/tools/net/NetClient.cpp:882](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetClient.cpp#L882)；`NetConnectionProbeResult NetClient::probeConnection(` | 连接探测 |
| <a id="evidence-v07"></a>V07 | [v1：src/tools/net/NetBatchDownloadDialog.cpp:424](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetBatchDownloadDialog.cpp#L424)；`void NetBatchDownloadDialog::buildUi(` | 下载界面、默认值与输出目录 |
| <a id="evidence-v08"></a>V08 | [v1：src/tools/net/NetBatchDownloadWorker.cpp:65](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetBatchDownloadWorker.cpp#L65)；`void NetBatchDownloadWorker::run(` | 批量下载、PV、重试与取消 |
| <a id="evidence-v09"></a>V09 | [v1：src/tools/net/NetClient.cpp:758](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetClient.cpp#L758)；`NetDownloadResult NetClient::downloadResourceToFile(` | 流式下载与原子文件 |
| <a id="evidence-v10"></a>V10 | [v1：src/tools/net/NetClient.cpp:486](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetClient.cpp#L486)；`bool packNetChartFolderZip(` | ZIP 内容与路径 |
| <a id="evidence-v11"></a>V11 | [v1：src/tools/net/NetBatchDownloadDialog.cpp:911](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetBatchDownloadDialog.cpp#L911)；`void NetBatchDownloadDialog::onlinePreview(` | 临时预览与缓存 |
| <a id="evidence-v12"></a>V12 | [v1：src/app/mainwindow/sections/document/MainWindow.DocumentFileFlow.cpp:592](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/app/mainwindow/sections/document/MainWindow.DocumentFileFlow.cpp#L592)；`bool MainWindow::DocumentSection::openOnlinePreviewAtPath(` | 预览文档打开与会话策略 |
| <a id="evidence-v13"></a>V13 | [v1：src/app/mainwindow/sections/document/MainWindow.DocumentAutosaveFlow.cpp:417](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/app/mainwindow/sections/document/MainWindow.DocumentAutosaveFlow.cpp#L417)；`state_.onlinePreviewDocument_` | 预览文档自动保存保护 |
| <a id="evidence-v14"></a>V14 | [v1：src/tools/net/NetBatchUploadScanner.cpp:48](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetBatchUploadScanner.cpp#L48)；`QList<NetUploadJob> scanNetUploadFolders(` | 上传素材扫描与去重 |
| <a id="evidence-v15"></a>V15 | [v1：src/tools/net/NetBatchUploadDialog.cpp:150](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetBatchUploadDialog.cpp#L150)；`StoredCredentials loadStoredCredentials(` | 账号存储、上传队列与失败重试 |
| <a id="evidence-v16"></a>V16 | [v1：src/tools/net/NetBatchUploadWorker.cpp:239](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetBatchUploadWorker.cpp#L239)；`UploadAttemptResult login(` | 登录、上传协议、节流与磁盘日志 |
| <a id="evidence-v17"></a>V17 | [v1：src/tools/net/NetUploadDiagnostics.cpp:84](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/tools/net/NetUploadDiagnostics.cpp#L84)；`NetUploadResponseAssessment assessNetUploadResponse(` | 上传响应分类与 Retry-After |
| <a id="evidence-v18"></a>V18 | [v1：src/app/mainwindow/sections/export/MainWindow.ExportSection.cpp:60](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/app/mainwindow/sections/export/MainWindow.ExportSection.cpp#L60)；`void MainWindow::ExportSection::onNetBatchDownload(` | 独立窗口与宿主回调 |
| <a id="evidence-v19"></a>V19 | [v1：src/app/mainwindow/sections/frame/MainWindow.BootstrapAndMenus.cpp:399](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/app/mainwindow/sections/frame/MainWindow.BootstrapAndMenus.cpp#L399)；`owner_.netBatchDownloadAction_ = new QAction(` | 下载菜单动作 |
| <a id="evidence-v20"></a>V20 | [v1：src/app/mainwindow/sections/frame/MainWindow.ExtensionHostRequests.cpp:1380](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/app/mainwindow/sections/frame/MainWindow.ExtensionHostRequests.cpp#L1380)；`net.batchUpload.open` | 上传内部命令 |
| <a id="evidence-v21"></a>V21 | [v1：src/extensions/ExtensionManager.cpp:2484](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/extensions/ExtensionManager.cpp#L2484)；`if (method == QStringLiteral("net/fetch")` | 通用 GET、下载与代理处理器 |
| <a id="evidence-v22"></a>V22 | [v1：src/extensions/EmbeddedExtensionRuntime.cpp:905](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/extensions/EmbeddedExtensionRuntime.cpp#L905)；`Q_INVOKABLE QJSValue fetch(` | miacode.net facade |
| <a id="evidence-v23"></a>V23 | [v1：src/extensions/ExtensionManager.cpp:1430](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/src/extensions/ExtensionManager.cpp#L1430)；`if (method.contains(QStringLiteral("WithoutPrompt")) && method.startsWith(QStringLiteral("net/")))` | 网络权限映射 |
| <a id="evidence-v24"></a>V24 | [v1：packages/miacode-extension-api/index.d.ts:151](https://github.com/fanfaredash/MiaCode/blob/6573efb611056bccd046dd17bc2f4cceda78bf88/packages/miacode-extension-api/index.d.ts#L151)；`export interface MiaCode` | 旧 SDK 类型面 |
| <a id="evidence-d01"></a>D01 | [dev：cmake/MiaCodeModules.cmake:574](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/cmake/MiaCodeModules.cmake#L574)；`miacode_add_module(miacode_media_tools` | 网络引擎所属静态库 |
| <a id="evidence-d02"></a>D02 | [dev：src/media_tools/net/NetClient.cpp:451](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/media_tools/net/NetClient.cpp#L451)；`QList<NetChartSummary> NetClient::queryCharts(` | dev 查询与传输 |
| <a id="evidence-d03"></a>D03 | [dev：src/media_tools/net/NetClient.h:83](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/media_tools/net/NetClient.h#L83)；`class NetClient` | dev 客户端公开接口 |
| <a id="evidence-d04"></a>D04 | [dev：src/media_tools/net/NetBatchDownloadWorker.cpp:58](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/media_tools/net/NetBatchDownloadWorker.cpp#L58)；`void NetBatchDownloadWorker::run(` | dev 下载 worker |
| <a id="evidence-d05"></a>D05 | [dev：src/media_tools/net/NetBatchUploadWorker.cpp:311](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/media_tools/net/NetBatchUploadWorker.cpp#L311)；`void NetBatchUploadWorker::run(` | dev 上传 worker |
| <a id="evidence-d06"></a>D06 | [dev：src/media_tools/net/NetBatchUploadScanner.cpp:48](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/media_tools/net/NetBatchUploadScanner.cpp#L48)；`QList<NetUploadJob> scanNetUploadFolders(` | dev 扫描器 |
| <a id="evidence-d07"></a>D07 | [dev：src/media_tools/net/NetUploadDiagnostics.cpp:84](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/media_tools/net/NetUploadDiagnostics.cpp#L84)；`NetUploadResponseAssessment assessNetUploadResponse(` | dev 响应分类器 |
| <a id="evidence-d08"></a>D08 | [dev：src/app/services/ApplicationServices.h:62](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/services/ApplicationServices.h#L62)；`class ApplicationServices` | 服务装配与 typed slots |
| <a id="evidence-d09"></a>D09 | [dev：src/app/services/ChartWorkspace.h:32](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/services/ChartWorkspace.h#L32)；`struct ChartWorkspaceSnapshot` | 文档权威与打开代次 |
| <a id="evidence-d10"></a>D10 | [dev：src/app/runtime/document/DocumentFileFlow.cpp:283](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/runtime/document/DocumentFileFlow.cpp#L283)；`void miacode::runtime::DocumentSessionHost::applyOpenedDocumentState(` | dev 文档打开副作用 |
| <a id="evidence-d11"></a>D11 | [dev：src/app/runtime/document/DocumentAutosave.cpp:383](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/runtime/document/DocumentAutosave.cpp#L383)；`void miacode::runtime::DocumentSessionHost::runAutosaveCheck(` | dev 自动保存 |
| <a id="evidence-d12"></a>D12 | [dev：src/app/ui/document/CommandService.cpp:18](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/ui/document/CommandService.cpp#L18)；`void CommandService::whenDocumentMayBeLeft(` | 离开文档保护 |
| <a id="evidence-d13"></a>D13 | [dev：src/app/services/JobProgressService.h:18](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/services/JobProgressService.h#L18)；`class JobProgressService` | 单任务进度展示 |
| <a id="evidence-d14"></a>D14 | [dev：src/app/services/UiRequestService.h:45](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/services/UiRequestService.h#L45)；`class UiRequestService` | 异步选择与确认端口 |
| <a id="evidence-d15"></a>D15 | [dev：src/tools/extensions/ExtensionProductBoundarySpec.cpp:28](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/tools/extensions/ExtensionProductBoundarySpec.cpp#L28)；`bool testArchiveBoundary(` | 扩展宿主归档边界 |
| <a id="evidence-d16"></a>D16 | [dev：docs/ops/DEPENDENCY_ALLOWLIST.md:49](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/docs/ops/DEPENDENCY_ALLOWLIST.md#L49)；`\| `Qt6::Network`` | 产品网络依赖现状 |
| <a id="evidence-d17"></a>D17 | [dev：src/app/services/update/NetworkUpdateFetcher.cpp:26](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/services/update/NetworkUpdateFetcher.cpp#L26)；`void NetworkUpdateFetcher::fetch(` | 更新检查网络传输 |
| <a id="evidence-d18"></a>D18 | [dev：cmake/devtools/specs/core.cmake:306](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/cmake/devtools/specs/core.cmake#L306)；`miacode_add_spec(net_client_spec` | net Spec 注册与过期注释 |
| <a id="evidence-d19"></a>D19 | [dev：docs/specs/ui/CURRENT_ARCHITECTURE_ZH.md:9](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/docs/specs/ui/CURRENT_ARCHITECTURE_ZH.md#L9)；`# 当前应用架构与所有权` | 当前 QML、services、runtime 架构 |
| <a id="evidence-d20"></a>D20 | [dev：src/app/ui/ApplicationContext.h:30](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/app/ui/ApplicationContext.h#L30)；`class ApplicationContext` | 现有 QML 服务入口 |
| <a id="evidence-d21"></a>D21 | [dev：src/tools/boundary/WebModuleBoundarySpec.cpp:50](https://github.com/fanfaredash/MiaCode/blob/cb52f6e8103e2773e839b797cdbd2522e780e2c7/src/tools/boundary/WebModuleBoundarySpec.cpp#L50)；`int main(` | Web 模块边界 Spec |
