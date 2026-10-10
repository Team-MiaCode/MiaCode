---
lifecycle: working
owner: src/app/services/api
last_verified: 2026-10-10
---

# Net 操作目录（生成）

来源：`tools/net-api/operations.json` 与 `schemas.json`。用生成器更新；不能独立手改。

`internal` 表示宿主内部实现；HTTP/CLI 开放状态以 adapters 和运行时 capability 为准。

| Operation ID | 用途 | HTTP（/api/v1） | 请求 → 结果 | 幂等 | 实施 | 验收 |
| --- | --- | --- | --- | --- | --- | --- |
| `api.capabilities` | 查询宿主能力与适配器状态 | GET `/capabilities` | EmptyRequest → CapabilitySet | natural | internal | TC17 |
| `net.providers.list` | 列出在线谱面平台 | GET `/net/providers` | EmptyRequest → ProviderList | natural | internal | TC17 |
| `net.probes.create` | 探测平台连通性 | POST `/net/probes` | ProviderRequest → JobHandle | optional | internal | TC05 |
| `net.queries.create` | 创建谱面查询任务 | POST `/net/queries` | ChartQueryRequest → JobHandle | optional | internal | TC01, TC02, TC03, TC04 |
| `net.queries.results` | 读取不可变查询结果分页 | GET `/net/queries/{queryRef}` | QueryPageRequest → ChartPage | natural | internal | TC03, TC17 |
| `net.downloads.create` | 下载谱面资源并按 manifest 发布 | POST `/net/downloads` | DownloadRequest → JobHandle | required | internal | TC06, TC07, TC08 |
| `net.uploads.scan` | 扫描授权目录并生成上传计划 | POST `/net/upload-plans` | ScanRequest → JobHandle | optional | internal | TC11 |
| `net.uploads.create` | 按冻结计划与顺序上传 | POST `/net/uploads` | UploadRequest → JobHandle | required | internal | TC12, TC13, TC14, TC15 |
| `net.accounts.login` | 创建隔离账号会话 | POST `/net/accounts` | AccountLoginRequest → JobHandle | none | internal | TC12, TC15 |
| `net.accounts.list` | 列出主体可使用的账号 | GET `/net/accounts` | EmptyRequest → AccountList | natural | internal | TC12, TC15 |
| `net.accounts.logout` | 撤销账号并取消关联传输 | DELETE `/net/accounts/{accountRef}` | AccountRefRequest → ReleaseResult | natural | internal | TC12, TC14 |
| `net.previews.prepare` | 准备在线预览缓存 | POST `/net/previews` | PreviewPrepareRequest → JobHandle | optional | internal | TC09 |
| `net.previews.open` | 校验文档身份并打开预览 | POST `/net/previews/{previewRef}/open` | PreviewOpenRequest → JobHandle | required | internal | TC10 |
| `net.previews.release` | 释放预览租约 | DELETE `/net/previews/{previewRef}` | PreviewRefRequest → ReleaseResult | natural | internal | TC09 |
| `document.snapshot` | 读取当前文档身份 | GET `/document/snapshot` | EmptyRequest → DocumentIdentity | natural | internal | TC10 |
| `net.settings.get` | 读取 Net 偏好 | GET `/net/settings` | EmptyRequest → NetSettings | natural | planned | TC20 |
| `net.settings.update` | 更新 Net 偏好 | PATCH `/net/settings` | NetSettingsPatch → NetSettings | natural | planned | TC20 |
| `jobs.list` | 列出主体任务 | GET `/jobs` | JobListRequest → JobPage | natural | internal | TC08, TC17 |
| `jobs.get` | 读取任务快照 | GET `/jobs/{jobId}` | JobRefRequest → JobSnapshot | natural | internal | TC08, TC17 |
| `jobs.items` | 读取任务项目分页 | GET `/jobs/{jobId}/items` | ItemPageRequest → ItemPage | natural | internal | TC08, TC17 |
| `jobs.events` | 增量读取任务事件 | GET `/jobs/{jobId}/events` | EventPageRequest → EventPage | natural | internal | TC08, TC17 |
| `jobs.cancel` | 请求取消任务 | POST `/jobs/{jobId}/cancel` | JobRefRequest → JobSnapshot | natural | internal | TC08, TC17 |
| `jobs.resume` | 按版本与解除条件恢复阻塞任务 | POST `/jobs/{jobId}/resume` | ResumeRequest → JobSnapshot | required | planned | TC08, TC13, TC14 |
| `jobs.retry` | 为可安全重试项目创建子任务 | POST `/jobs/{jobId}/retry` | RetryRequest → JobHandle | required | planned | TC08, TC14 |
| `jobs.diagnostics` | 读取脱敏任务诊断 | GET `/jobs/{jobId}/diagnostics` | JobRefRequest → DiagnosticSummary | natural | planned | TC15 |
| `files.grants.request` | 请求本机目录授权 | POST `/files/grants` | FileGrantRequest → JobHandle | optional | planned | TC06, TC16 |
| `files.artifacts.content` | 读取授权产物内容 | GET `/files/artifacts/{artifactRef}/content` | ArtifactRefRequest → BinaryContent | natural | planned | TC06, TC16 |
| `network.targets.request` | 请求网络目标授权 | POST `/network/target-grants` | TargetGrantRequest → JobHandle | optional | planned | TC18 |
| `network.http.fetch` | 向授权目标执行文本 GET | POST `/network/fetch` | HttpFetchRequest → JobHandle | optional | planned | TC18 |
| `network.http.download` | 向授权目标执行流式文件 GET | POST `/network/downloads` | HttpDownloadRequest → JobHandle | required | planned | TC06, TC18 |
| `network.proxy.get` | 读取请求域代理 | GET `/network/proxy/{profileId}` | ProxyRefRequest → ProxyProfile | natural | planned | TC18 |
| `network.proxy.set` | 更新请求域代理 | PUT `/network/proxy/{profileId}` | ProxyProfileRequest → JobHandle | optional | planned | TC18 |
| `gateway.pairings.create` | 在配对窗口创建挑战 | POST `/pairings` | PairingRequest → PairingChallenge | none | planned | TC16 |
| `gateway.pairings.exchange` | 兑换用户批准的客户端令牌 | POST `/pairings/{pairingRef}/exchange` | PairingExchangeRequest → PairingResult | none | planned | TC16 |
