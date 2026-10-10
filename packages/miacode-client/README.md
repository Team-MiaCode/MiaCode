# MiaCode 应用 API 客户端

本包定义新的应用 API，按稳定 operationId 调用。它的 transport 可接桌面宿主、未来本机 HTTP 网关或其他平台适配器；业务 DTO 使用同一份目录生成。

当前提供 TypeScript 类型、注入式 `ApiTransport`、HTTP 请求编码、任务等待和查询分页。当前桌面产品没有开放本机 HTTP 网关或 CLI；`HttpTransport` 与对应路由是待接入适配器的契约实现，不能据此直接访问正在运行的 MiaCode。运行时必须查询 `api.capabilities`，核对 `available` 和 `adapters`。

```ts
import { MiaCodeClient, HttpTransport } from "@miacode/client";

// baseUrl/token 由完成配对的宿主提供；以下用于未来兼容网关。
const client = new MiaCodeClient(new HttpTransport(baseUrl, () => token));
const accepted = await client.call("net.queries.create", {
  providerId: "majdata", uploader: "example-user", tag: "event",
  timeZone: "Asia/Shanghai", sort: "uploaded_desc"
});
const job = await client.waitJob(accepted.jobId, { signal, timeoutMs: 120_000 });
if (job.state !== "succeeded") throw new Error(job.state);
// JobSnapshot.result 需要按源操作的 jobResultSchema 校验/收窄后使用。
```

`waitJob` 在 blocked/awaiting_user 时返回，交由应用展示用户操作；不会自动恢复、重试或取消远端任务。轮询默认 500 ms，允许 500–1000 ms。传入 signal 可停止本次等待；取消任务需显式调用 `jobs.cancel`。所有 uint64 字段是十进制字符串。

目录与类型生成：从仓库根运行 `python scripts/api/generate_net_api.py`；一致性检查加 `--check`。本包构建与验证：`npm ci --ignore-scripts`、`npm run build`、`npm test`。生成文件不能独立修改；新增操作先更新 `tools/net-api/operations.json` 和 `schemas.json`，再补宿主行为及权限验收。
