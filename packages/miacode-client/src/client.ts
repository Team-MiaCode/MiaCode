import { operations, type OperationId, type OperationMap, type JobSnapshot, type ChartPage } from "./generated.js";
export * from "./generated.js";

export interface CallOptions {
  requestId?: string;
  idempotencyKey?: string;
  signal?: AbortSignal;
}

export interface ApiTransport {
  call<K extends OperationId>(operation: K, request: OperationMap[K]["request"], options?: CallOptions): Promise<OperationMap[K]["result"]>;
}

export class ApiError extends Error {
  constructor(public readonly code: string, message: string, public readonly status?: number,
              public readonly requestId?: string) { super(message); this.name = "ApiError"; }
}

function record(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

/** HTTP adapter for a future compatible host. Construction does not imply endpoint availability. */
export class HttpTransport implements ApiTransport {
  constructor(private readonly baseUrl: string, private readonly token: () => string,
              private readonly fetcher: typeof fetch = globalThis.fetch) {}

  async call<K extends OperationId>(operation: K, request: OperationMap[K]["request"], options: CallOptions = {}): Promise<OperationMap[K]["result"]> {
    const descriptor = operations[operation];
    if (descriptor.idempotency === "required" && !options.idempotencyKey)
      throw new ApiError("request.invalid", "An idempotency key is required.");
    const safeHeader = /^[A-Za-z0-9_.:-]{1,128}$/;
    if ((options.requestId && !safeHeader.test(options.requestId)) || (options.idempotencyKey && !safeHeader.test(options.idempotencyKey)))
      throw new ApiError("request.invalid", "Invalid request or idempotency key.");
    if (!record(request)) throw new ApiError("request.invalid", "Request must be an object.");
    const fields: Record<string, unknown> = { ...request };
    const route = descriptor.path.replace(/\{([^}]+)\}/g, (_match, name: string) => {
      const value = fields[name];
      if (typeof value !== "string" || !/^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$/.test(value))
        throw new ApiError("request.invalid", "Invalid path reference.");
      delete fields[name];
      return encodeURIComponent(value);
    });
    const url = new URL(this.baseUrl.replace(/\/$/, "") + "/api/v1" + route);
    const headers: Record<string, string> = { Accept: descriptor.binary ? "application/octet-stream, application/json" : "application/json" };
    const token = this.token();
    if (token) headers.Authorization = "Bearer " + token;
    if (options.requestId) headers["X-Request-Id"] = options.requestId;
    if (options.idempotencyKey) headers["Idempotency-Key"] = options.idempotencyKey;
    let body: string | undefined;
    if (descriptor.method === "GET") {
      for (const [name, value] of Object.entries(fields)) {
        if (value !== undefined) url.searchParams.set(name, String(value));
      }
    } else if (Object.keys(fields).length) {
      headers["Content-Type"] = "application/json";
      body = JSON.stringify(fields);
    }
    const response = await this.fetcher(url, { method: descriptor.method, headers, body, signal: options.signal,
      credentials: "omit", redirect: "error" });
    const contentType = response.headers.get("content-type")?.split(";")[0]?.trim().toLowerCase();
    if (descriptor.binary && response.ok && contentType && contentType !== "application/json" && contentType !== "text/html")
      return await response.arrayBuffer() as OperationMap[K]["result"];
    if (contentType !== "application/json") throw new ApiError("transport.invalid_response", "Expected an API envelope.", response.status);
    const envelope: unknown = await response.json();
    if (!record(envelope) || envelope.apiVersion !== "1.0" || typeof envelope.requestId !== "string" || typeof envelope.ok !== "boolean")
      throw new ApiError("transport.invalid_response", "Invalid API envelope.", response.status);
    if (!envelope.ok) {
      const error = envelope.error;
      if (!record(error) || typeof error.code !== "string" || typeof error.message !== "string")
        throw new ApiError("transport.invalid_response", "Invalid API error.", response.status, envelope.requestId);
      throw new ApiError(error.code, error.message, response.status, envelope.requestId);
    }
    if (!response.ok || !("result" in envelope)) throw new ApiError("transport.invalid_response", "Invalid successful response.", response.status, envelope.requestId);
    return envelope.result as OperationMap[K]["result"];
  }
}

function wait(ms: number, signal?: AbortSignal): Promise<void> {
  return new Promise((resolve, reject) => {
    const abort = () => { clearTimeout(timer); signal?.removeEventListener("abort", abort); reject(signal?.reason ?? new DOMException("Aborted", "AbortError")); };
    const timer = setTimeout(() => { signal?.removeEventListener("abort", abort); resolve(); }, ms);
    if (signal?.aborted) abort();
    else signal?.addEventListener("abort", abort, { once: true });
  });
}

export class MiaCodeClient {
  constructor(private readonly transport: ApiTransport) {}
  call<K extends OperationId>(operation: K, request: OperationMap[K]["request"], options?: CallOptions): Promise<OperationMap[K]["result"]> {
    return this.transport.call(operation, request, options);
  }

  /** Returns blocked/awaiting_user to the caller; it never resumes a job implicitly. */
  async waitJob(jobId: string, options: { signal?: AbortSignal; timeoutMs?: number; pollMs?: number } = {}): Promise<JobSnapshot> {
    const pollMs = options.pollMs ?? 500;
    const timeoutMs = options.timeoutMs ?? 120_000;
    if (!Number.isFinite(pollMs) || pollMs < 500 || pollMs > 1000 || !Number.isFinite(timeoutMs) || timeoutMs < 1)
      throw new ApiError("request.invalid", "Invalid polling budget.");
    const controller = new AbortController();
    const aborted = () => controller.abort(options.signal?.reason);
    const timer = setTimeout(() => controller.abort(new ApiError("job.wait_timeout", "Task polling budget expired.")), timeoutMs);
    if (options.signal?.aborted) aborted();
    else options.signal?.addEventListener("abort", aborted, { once: true });
    try {
      while (true) {
        controller.signal.throwIfAborted();
        const job = await this.call("jobs.get", { jobId }, { signal: controller.signal });
        if (["succeeded", "partial", "failed", "cancelled", "blocked", "awaiting_user"].includes(job.state)) return job;
        await wait(pollMs, controller.signal);
      }
    } finally {
      clearTimeout(timer);
      options.signal?.removeEventListener("abort", aborted);
    }
  }

  async *queryPages(queryRef: string, options: { limit?: number; signal?: AbortSignal } = {}): AsyncGenerator<ChartPage> {
    let cursor = "";
    const seen = new Set<string>();
    do {
      options.signal?.throwIfAborted();
      const page = await this.call("net.queries.results", { queryRef, cursor, limit: options.limit ?? 100 }, { signal: options.signal });
      yield page;
      cursor = page.nextCursor ?? "";
      if (cursor && seen.has(cursor)) throw new ApiError("transport.invalid_response", "Repeated page cursor.");
      if (cursor) seen.add(cursor);
    } while (cursor);
  }
}
