import test from "node:test";
import assert from "node:assert/strict";
import { HttpTransport, MiaCodeClient, ApiError } from "../dist/client.js";

function response(result, status = 200) {
  return new Response(JSON.stringify({ apiVersion: "1.0", requestId: "req_test", ok: true, result }),
    { status, headers: { "Content-Type": "application/json; charset=utf-8" } });
}

test("HTTP routing separates references, query strings, body and secrets", async () => {
  const seen = [];
  const transport = new HttpTransport("http://127.0.0.1:34567", () => "token", async (url, options) => {
    seen.push({ url: String(url), options });
    return response({});
  });
  await transport.call("net.queries.results", { queryRef: "query_a", cursor: "cursor_1", limit: 20 });
  assert.equal(seen[0].url, "http://127.0.0.1:34567/api/v1/net/queries/query_a?cursor=cursor_1&limit=20");
  assert.equal(seen[0].options.body, undefined);
  await transport.call("net.accounts.login", { username: "a", password: "secret", remember: false });
  assert.ok(!seen[1].url.includes("secret"));
  assert.equal(JSON.parse(seen[1].options.body).password, "secret");
  assert.equal(seen[1].options.credentials, "omit");
  assert.equal(seen[1].options.redirect, "error");
  await assert.rejects(transport.call("jobs.get", { jobId: "../accounts" }), { code: "request.invalid" });
  await assert.rejects(transport.call("net.downloads.create", { chartIds: ["a"], destination: { kind: "managed" } }), { code: "request.invalid" });
  assert.equal(seen.length, 2);
});

test("envelopes distinguish an upstream job failure from an API failure", async () => {
  const transport = new HttpTransport("http://localhost:1", () => "", async () => response({ state: "failed", error: { code: "upstream.auth_required" } }));
  assert.equal((await transport.call("jobs.get", { jobId: "job_a" })).state, "failed");
  const denied = new HttpTransport("http://localhost:1", () => "", async () => new Response(JSON.stringify({
    apiVersion: "1.0", requestId: "request_denied", ok: false, error: { code: "permission.denied", message: "Denied" }
  }), { status: 403, headers: { "content-type": "application/json" } }));
  await assert.rejects(denied.call("jobs.get", { jobId: "job_a" }), { code: "permission.denied", status: 403, requestId: "request_denied" });
});

test("HTML and malformed envelopes do not become successful artifacts", async () => {
  for (const value of [new Response("<html>challenge</html>", { headers: { "content-type": "text/html" } }),
    new Response('{"ok":true}', { headers: { "content-type": "application/json" } })]) {
    const transport = new HttpTransport("http://localhost:1", () => "", async () => value);
    await assert.rejects(transport.call("files.artifacts.content", { artifactRef: "artifact_a" }), { code: "transport.invalid_response" });
  }
  const bytes = new HttpTransport("http://localhost:1", () => "", async () => new Response(new Uint8Array([1, 2]), { headers: { "content-type": "application/octet-stream" } }));
  assert.deepEqual(new Uint8Array(await bytes.call("files.artifacts.content", { artifactRef: "artifact_a" })), new Uint8Array([1, 2]));
});

test("waiting exposes user decisions and aborts a stalled call at its deadline", async () => {
  const blocked = new MiaCodeClient({ call: async () => ({ state: "blocked" }) });
  assert.equal((await blocked.waitJob("job_a")).state, "blocked");
  const waiting = new MiaCodeClient({ call: async (_operation, _request, options) => new Promise((_resolve, reject) => {
    options.signal.addEventListener("abort", () => reject(options.signal.reason), { once: true });
  }) });
  await assert.rejects(waiting.waitJob("job_a", { timeoutMs: 20 }), { code: "job.wait_timeout" });
  const controller = new AbortController();
  controller.abort(new ApiError("caller.cancelled", "Cancelled"));
  await assert.rejects(waiting.waitJob("job_a", { signal: controller.signal }), { code: "caller.cancelled" });
});

test("query pagination detects repeated cursors", async () => {
  const client = new MiaCodeClient({ call: async () => ({ items: [], nextCursor: "cursor_same" }) });
  const pages = client.queryPages("query_a");
  await pages.next();
  await pages.next();
  await assert.rejects(pages.next(), { code: "transport.invalid_response" });
});
