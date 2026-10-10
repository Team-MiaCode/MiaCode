#!/usr/bin/env python3
"""Generate API artifacts from the reviewed Net catalog. No third-party runtime needed."""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/net-api"


def load_sources():
    catalog = json.loads((SOURCE / "operations.json").read_text(encoding="utf-8"))
    schemas = json.loads((SOURCE / "schemas.json").read_text(encoding="utf-8"))
    inventory = json.loads((ROOT / "docs/specs/net/net-migration-inventory.json").read_text(encoding="utf-8-sig"))
    operations = catalog["operations"]
    ids = [op["id"] for op in operations]
    assert len(ids) == len(set(ids)), "duplicate operation ID"
    assert len({(op["method"], op["path"]) for op in operations}) == len(ids), "duplicate route"
    assert set(ids) == {op["id"] for op in inventory["proposed_api"]["operations"]}, "inventory/catalog drift"
    tests = {case["id"] for case in inventory["acceptance_cases"]}
    features = {feature["id"] for feature in inventory["features"]}
    defs = schemas["$defs"]
    for op in operations:
        assert op["acceptance"] and set(op["acceptance"]) <= tests, op["id"]
        assert set(op["featureIds"]) <= features, op["id"]
        for key in ("requestSchema", "resultSchema", "jobResultSchema"):
            if key in op:
                assert op[key] in defs, f"{op['id']}: unresolved {key}"
        request = defs[op["requestSchema"]]
        assert request.get("additionalProperties") is False, op["id"]
        assert all("default" not in request["properties"][key] for key in request["required"]), "required fields cannot be defaulted"
        for parameter in re.findall(r"\{([^}]+)\}", op["path"]):
            assert parameter in request["properties"] and parameter in request["required"], op["id"]
        assert op["idempotency"] in {"required", "optional", "natural", "none"}, op["id"]
        assert op["schemaVersion"] == catalog["schemaVersion"], op["id"]
        assert op["implementation"] in {"planned", "internal", "delivered"}, op["id"]
        if op["implementation"] == "planned":
            assert not any(op["adapters"].values()), op["id"]
        assert op["owner"] and op["introducedIn"], op["id"]
        for secret in op["secretFields"]:
            assert request["properties"][secret].get("x-secret") is True, op["id"]
            assert op["idempotency"] == "none", "secret request must not enter the replay ledger"

    supported = {"$ref", "type", "properties", "required", "additionalProperties", "items", "enum", "const",
                 "anyOf", "oneOf", "minLength", "maxLength", "pattern", "minimum", "maximum", "minItems",
                 "maxItems", "uniqueItems", "minProperties", "default", "format", "writeOnly", "description",
                 "contentEncoding", "x-secret", "x-trim", "x-normalize", "x-format", "x-uint64", "x-binary-stream"}

    def verify(node):
        assert set(node) <= supported, f"unsupported validator keyword: {set(node) - supported}"
        if "$ref" in node:
            assert node["$ref"].startswith("#/$defs/") and node["$ref"][8:] in defs, node["$ref"]
        assert node.get("format") in {None, "date", "date-time", "uri"}
        assert node.get("x-format") in {None, "time-zone"}
        assert node.get("x-normalize") in {None, "tag"}
        for child in node.get("properties", {}).values():
            verify(child)
        if "items" in node:
            verify(node["items"])
        for keyword in ("anyOf", "oneOf"):
            for child in node.get(keyword, []):
                verify(child)
    for definition in defs.values():
        verify(definition)
    return catalog, schemas


def remap_refs(value):
    if isinstance(value, dict):
        return {key: (item.replace("#/$defs/", "#/components/schemas/") if key == "$ref" else remap_refs(item))
                for key, item in value.items()}
    if isinstance(value, list):
        return [remap_refs(item) for item in value]
    return value


def envelope(result):
    return {"type": "object", "required": ["apiVersion", "requestId", "ok", "result"], "properties": {
        "apiVersion": {"const": "1.0", "type": "string"}, "requestId": {"type": "string"},
        "ok": {"const": True, "type": "boolean"}, "result": result, "meta": {"type": "object"}},
        "additionalProperties": True}


def openapi(catalog, schemas):
    defs = remap_refs(schemas["$defs"])
    defs["ErrorEnvelope"] = {"type": "object", "required": ["apiVersion", "requestId", "ok", "error"],
        "properties": {"apiVersion": {"type": "string", "const": "1.0"}, "requestId": {"type": "string"},
                       "ok": {"type": "boolean", "const": False}, "error": {"$ref": "#/components/schemas/Error"}}}
    paths = {}
    for op in catalog["operations"]:
        request = copy.deepcopy(defs[op["requestSchema"]])
        parameters = [{"in": "header", "name": "X-Request-Id", "required": False,
                       "schema": {"type": "string", "minLength": 1, "maxLength": 128, "pattern": "^[A-Za-z0-9_.:-]+$"}}]
        if op["idempotency"] in {"required", "optional"}:
            parameters.append({"in": "header", "name": "Idempotency-Key", "required": op["idempotency"] == "required",
                "schema": {"type": "string", "minLength": 1, "maxLength": 128, "pattern": "^[A-Za-z0-9_.:-]+$"}})
        for name in re.findall(r"\{([^}]+)\}", op["path"]):
            parameters.append({"in": "path", "name": name, "required": True, "schema": request["properties"].pop(name)})
            request["required"].remove(name)
        endpoint = {"operationId": op["id"], "summary": op["summary"], "parameters": parameters,
                    "security": [] if op["id"].startswith("gateway.") else [{"appBearer": []}],
                    "x-scopes": op["scopes"], "x-authorization-rule": op["authorizationRule"],
                    "x-implementation": op["implementation"], "x-adapters": op["adapters"],
                    "x-owner": op["owner"], "x-idempotency": op["idempotency"], "x-acceptance": op["acceptance"]}
        if op["async"]:
            endpoint["x-job-kind"] = op["jobKind"]
            endpoint["x-job-result-schema"] = {"$ref": f"#/components/schemas/{op['jobResultSchema']}"}
        if op["method"] == "GET":
            for name, prop in request["properties"].items():
                parameters.append({"in": "query", "name": name, "required": name in request["required"], "schema": prop})
        elif request["properties"]:
            endpoint["requestBody"] = {"required": True, "content": {"application/json": {"schema": request}}}
        content = ({"application/octet-stream": {"schema": {}}} if op["resultSchema"] == "BinaryContent"
                   else {"application/json": {"schema": envelope({"$ref": f"#/components/schemas/{op['resultSchema']}"})}})
        endpoint["responses"] = {"202" if op["async"] else "200": {"description": "Accepted task" if op["async"] else "Success",
            "content": content}, "default": {"description": "API error envelope", "content": {
                "application/json": {"schema": {"$ref": "#/components/schemas/ErrorEnvelope"}}}}}
        paths.setdefault(op["path"], {})[op["method"].lower()] = endpoint
    return {"openapi": "3.1.1", "info": {"title": "MiaCode Net API", "version": catalog["apiVersion"],
        "description": "Generated contract. HTTP/CLI adapters are not delivered; inspect x-adapters and runtime capabilities."},
        "servers": [{"url": "/api/v1"}], "paths": paths,
        "components": {"schemas": defs, "securitySchemes": {"appBearer": {"type": "http", "scheme": "bearer"}}}}


def ts_type(schema):
    if "$ref" in schema:
        return schema["$ref"].split("/")[-1]
    for key in ("oneOf", "anyOf"):
        if key in schema:
            return " | ".join(ts_type(item) for item in schema[key])
    if "const" in schema:
        return json.dumps(schema["const"])
    if "enum" in schema:
        return " | ".join(json.dumps(item) for item in schema["enum"])
    kind = schema.get("type")
    if isinstance(kind, list):
        return " | ".join(ts_type({**schema, "type": item}) for item in kind)
    if kind == "object":
        fields = [f'{json.dumps(key)}{"" if key in schema.get("required", []) else "?"}: {ts_type(value)}'
                  for key, value in schema.get("properties", {}).items()]
        if schema.get("additionalProperties", True):
            fields.append("[key: string]: unknown")
        return "{ " + "; ".join(fields) + " }"
    if kind == "array":
        return f"Array<{ts_type(schema['items'])}>"
    return {"string": "string", "integer": "number", "number": "number", "boolean": "boolean", "null": "null"}.get(kind, "unknown")


def typescript(catalog, schemas):
    lines = ["// Generated by scripts/api/generate_net_api.py. Do not edit.",
             "// This defines the API contract; availability is determined by the connected host."]
    for name, schema in schemas["$defs"].items():
        lines.append(f"export type {name} = {ts_type(schema)};")
    lines.append("export interface OperationMap {")
    for op in catalog["operations"]:
        result = "ArrayBuffer" if op["resultSchema"] == "BinaryContent" else op["resultSchema"]
        lines.append(f'  "{op["id"]}": {{ request: {op["requestSchema"]}; result: {result} }};')
    lines.extend(["}", "export type OperationId = keyof OperationMap;", "export const operations = {"])
    for op in catalog["operations"]:
        data = {key: op[key] for key in ("method", "path", "async", "idempotency", "adapters")}
        data["binary"] = op["resultSchema"] == "BinaryContent"
        lines.append(f'  "{op["id"]}": {json.dumps(data, separators=(",", ":"))},')
    lines.extend(["} as const;", ""])
    return "\n".join(lines)


def artifacts(catalog, schemas):
    dump = lambda value: json.dumps(value, ensure_ascii=False, indent=2) + "\n"
    packed = json.dumps({"catalog": catalog, "schemas": schemas}, ensure_ascii=True, separators=(",", ":"))
    assert ')miacode_api"' not in packed
    # MSVC limits a single string literal; keep chunks separate, then concatenate at runtime.
    chunks = [packed[offset:offset + 8000] for offset in range(0, len(packed), 8000)]
    header = ('// Generated by scripts/api/generate_net_api.py. Do not edit.\n#pragma once\n\n'
              'namespace miacode::api::generated {\ninline constexpr const char* catalogChunks[] = {\n'
              + ''.join('R"miacode_api(' + chunk + ')miacode_api",\n' for chunk in chunks) + '};\n}\n')
    doc = ["---", "lifecycle: working", "owner: src/app/services/api", f"last_verified: {catalog['verifiedOn']}", "---", "",
           "# Net 操作目录（生成）", "", "来源：`tools/net-api/operations.json` 与 `schemas.json`。用生成器更新；不能独立手改。",
           "", "`internal` 表示宿主内部实现；HTTP/CLI 开放状态以 adapters 和运行时 capability 为准。", "",
           "| Operation ID | 用途 | HTTP（/api/v1） | 请求 → 结果 | 幂等 | 实施 | 验收 |", "| --- | --- | --- | --- | --- | --- | --- |"]
    for op in catalog["operations"]:
        if op["id"].startswith(("net.uploads.", "net.accounts.")):
            continue
        acceptance = [case for case in op["acceptance"] if case not in {"TC11", "TC12", "TC13", "TC14", "TC15"}]
        doc.append(f'| `{op["id"]}` | {op["summary"]} | {op["method"]} `{op["path"]}` | '
                   f'{op["requestSchema"]} → {op["resultSchema"]} | {op["idempotency"]} | {op["implementation"]} | {", ".join(acceptance) or "—"} |')
    return {ROOT / "src/app/services/api/generated/NetApiCatalogData.h": header,
            ROOT / "docs/specs/net/generated/net-openapi.json": dump(openapi(catalog, schemas)),
            ROOT / "docs/specs/net/generated/NET_OPERATION_CATALOG_ZH.md": "\n".join(doc) + "\n",
            ROOT / "packages/miacode-client/src/generated.ts": typescript(catalog, schemas)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail on catalog/schema/generated artifact drift")
    args = parser.parse_args()
    catalog, schemas = load_sources()
    changed = []
    for path, text in artifacts(catalog, schemas).items():
        content = text.encode("utf-8")
        if path.exists() and path.read_text(encoding="utf-8") == text:
            continue
        changed.append(str(path.relative_to(ROOT)))
        if not args.check:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
    if args.check and changed:
        print("Net API artifacts out of date:\n" + "\n".join(changed), file=sys.stderr)
        return 1
    print(f"Net API: {len(catalog['operations'])} operations, {len(schemas['$defs'])} schemas; "
          + ("consistent" if args.check else f"updated {len(changed)} artifacts"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
