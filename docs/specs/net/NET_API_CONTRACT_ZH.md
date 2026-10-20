# Net 接口

接口与请求结构定义于 `tools/net-api/operations.json`、`tools/net-api/schemas.json`。
`ApiCatalog` 校验请求与结果，`ApiDispatcher` 处理权限与任务分发，`NetService` 提供桌面调用。
SDK 位于 `packages/miacode-client/`，可用操作由运行时 capability 返回。

接口数据由 `scripts/api/generate_net_api.py` 生成：

- `src/app/services/api/generated/NetApiCatalogData.h`
- `packages/miacode-client/src/generated.ts`
- `docs/specs/net/generated/net-openapi.json`

## 桌面功能

- 工具箱和工具菜单使用“谱面下载”，打开 `net-download` 标签页。共享应用模型保留查询和任务，重复打开复用标签。
- `net.downloads.create` 使用目录授权或托管目录；资源流写入 QSaveFile，在同一输出根的暂存目录计算 SHA-256、写 manifest 并发布。`zipArtifacts[]` 表示各谱面目录中的 `download.zip`；ZIP 包含所选 PV，`legacy_triplet` 保留三资源模式。冲突默认 fail，可指定 replace。
- 下载恢复由可信桌面宿主使用版本化任务控制，继承原任务引用，保留成功 manifest 和输出。
- `net.previews.prepare` 按主体、chartId、remoteVersion、includeVideo 和一小时有效期复用缓存，PV absent 属于完整缓存；prepare 返回句柄，open 独立使用文档身份和离开文档决策。release 撤销句柄，缓存目录由 provider 生命周期持有，已打开媒体保持文件可用。
- `document.snapshot` 返回 workspaceId、documentOpenGeneration、revision、dirty、hasDocument、origin。net_preview 来源参与文档、运行时、会话和保存处理；保存通过另存为建立 local 文档。跨目录另存为迁移音轨、背景、PV、`&video=` 引用文件与 `.miacode` 项目设置；外部视频转为副本目录内的相对引用。资源和谱面写入成功后更新文档路径与保存点，资源失败恢复目标原有文件。

运行时任务和幂等记录归当前进程，capability 报告 process_lifetime。
