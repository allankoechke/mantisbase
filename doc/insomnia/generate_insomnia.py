#!/usr/bin/env python3
"""Generate an Insomnia v4 export from doc/openapi.yaml."""

from __future__ import annotations

import json
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
OPENAPI_PATH = ROOT / "doc" / "openapi.yaml"
OUTPUT_PATH = Path(__file__).resolve().parent / "mantisbase.insomnia.json"

TAG_ORDER = [
    "Health",
    "Auth",
    "Entities",
    "Schemas",
    "Files",
    "Realtime",
    "System",
]

# OpenAPI path/template -> Insomnia URL with environment variables
PATH_VAR_MAP = {
    "entity_name": "{{ _.entity_name }}",
    "schema_name_or_id": "{{ _.schema_name }}",
    "id": "{{ _.record_id }}",
    "entity": "{{ _.entity_name }}",
    "file": "{{ _.file_name }}",
    "provider": "{{ _.oauth_provider }}",
}

# Sample JSON bodies for common operations
BODY_SAMPLES: dict[str, object] = {
    "LoginRequest": {
        "identity": "{{ _.admin_email }}",
        "password": "{{ _.admin_password }}",
    },
    "EntityRecord_admin": {
        "email": "admin@example.com",
        "password": "changeme123",
    },
    "EntityRecord_user": {
        "name": "Jane Doe",
        "email": "jane@example.com",
        "password": "changeme123",
    },
    "EntityRecord_post": {
        "title": "Hello World",
        "body": "Sample post content",
    },
    "EntitySchemaCreate": {
        "name": "posts",
        "type": "base",
        "list": {"mode": "public", "expr": ""},
        "get": {"mode": "public", "expr": ""},
        "add": {"mode": "auth", "expr": ""},
        "update": {"mode": "auth", "expr": ""},
        "delete": {"mode": "", "expr": ""},
        "fields": [
            {"name": "title", "type": "string", "required": True},
            {"name": "body", "type": "string"},
        ],
    },
    "EntitySchemaPatch": {
        "fields": [
            {"name": "summary", "type": "string", "required": False},
        ],
    },
    "ApiKeyCreateRequest": {
        "label": "Insomnia test key",
        "permissions": [],
    },
    "AppSettingsPatch": {
        "orgName": "ACME Corp",
        "siteDomain": "https://acme.example.com",
        "maxFileSize": 10485760,
    },
    "OAuthProviderCreate": {
        "name": "google",
        "client_id": "your-client-id",
        "client_secret": "your-client-secret",
    },
    "OAuthEntityConfig": {
        "entity_name": "{{ _.entity_name }}",
        "provider_id": "{{ _.oauth_provider_id }}",
    },
    "RealtimeSessionUpdate": {
        "client_id": "{{ _.sse_client_id }}",
        "topics": ["{{ _.entity_name }}"],
    },
    "OAuthLink": {
        "code": "oauth-authorization-code",
        "state": "oauth-state",
    },
}

# Which schema sample to use per path/method
BODY_HINTS: dict[tuple[str, str], str] = {
    ("/auth/{entity_name}/login", "post"): "LoginRequest",
    ("/sys/admins/login", "post"): "LoginRequest",
    ("/sys/admins/setup", "post"): "EntityRecord_admin",
    ("/sys/admins", "post"): "EntityRecord_admin",
    ("/entities/{entity_name}", "post"): "EntityRecord_post",
    ("/schemas", "post"): "EntitySchemaCreate",
    ("/schemas/{schema_name_or_id}", "patch"): "EntitySchemaPatch",
    ("/sys/settings/config", "patch"): "AppSettingsPatch",
    ("/sys/oauth/providers", "post"): "OAuthProviderCreate",
    ("/sys/oauth/entity-config", "post"): "OAuthEntityConfig",
    ("/sys/oauth/entity-config", "delete"): "OAuthEntityConfig",
    ("/auth/{entity_name}/api-keys", "post"): "ApiKeyCreateRequest",
    ("/sys/api-keys", "post"): "ApiKeyCreateRequest",
    ("/auth/{entity_name}/oauth/link/{provider}", "post"): "OAuthLink",
    ("/realtime", "post"): "RealtimeSessionUpdate",
}

# Endpoints requiring admin bearer token
ADMIN_AUTH_PREFIXES = (
    "/schemas",
    "/sys/",
)

# Endpoints requiring user/entity bearer (when not public)
USER_AUTH_PREFIXES = (
    "/auth/{entity_name}/refresh",
    "/auth/{entity_name}/logout",
    "/auth/{entity_name}/api-keys",
    "/auth/{entity_name}/oauth/link",
    "/auth/{entity_name}/oauth/accounts",
)

PUBLIC_PATHS = {
    "/health",
    "/auth/verify",
    "/auth/{entity_name}/login",
    "/auth/{entity_name}/oauth/authorize/{provider}",
    "/auth/{entity_name}/oauth/callback/{provider}",
    "/auth/{entity_name}/oauth/providers",
    "/sys/admins/login",
    "/sys/admins/setup",
    "/realtime",
    "/realtime/ws",
}


def uid(prefix: str) -> str:
    return f"{prefix}_{uuid.uuid4().hex[:12]}"


def openapi_path_to_url(path: str) -> str:
    url = "{{ _.base_url }}" + path
    for key, var in PATH_VAR_MAP.items():
        url = url.replace("{" + key + "}", var)
    return url


def needs_admin_auth(path: str, operation: dict, spec: dict) -> bool:
    if path in PUBLIC_PATHS:
        return False
    security = operation.get("security")
    if security == []:
        return False
    if security is None:
        security = spec.get("security", [])
    if security == []:
        return False
    return path.startswith(ADMIN_AUTH_PREFIXES)


def needs_user_auth(path: str, operation: dict) -> bool:
    if path in PUBLIC_PATHS:
        return False
    for prefix in USER_AUTH_PREFIXES:
        if path.startswith(prefix):
            return True
    return False


def resolve_query_params(operation: dict) -> list[dict]:
    params = []
    for param in operation.get("parameters", []):
        if param.get("in") != "query":
            continue
        name = param["name"]
        if name == "limit":
            value = "{{ _.limit }}"
        elif name == "after":
            value = "{{ _.cursor }}"
        elif name == "filter":
            value = '{{ _.filter }}'
        elif name == "topics":
            value = "{{ _.entity_name }}"
        elif name == "search":
            value = "database"
        elif name == "level":
            value = "warn"
        elif name == "min_level":
            value = "warn"
        else:
            example = param.get("schema", {}).get("example")
            value = str(example) if example is not None else ""
        params.append({"name": name, "value": value, "disabled": value == ""})
    return params


def resolve_body(path: str, method: str, operation: dict) -> dict:
    hint = BODY_HINTS.get((path, method))
    if hint and hint in BODY_SAMPLES:
        return {
            "mimeType": "application/json",
            "text": json.dumps(BODY_SAMPLES[hint], indent=2),
        }

    request_body = operation.get("requestBody")
    if not request_body:
        return {}

    content = request_body.get("content", {})
    if "application/json" in content:
        schema = content["application/json"].get("schema", {})
        ref = schema.get("$ref", "")
        if ref:
            name = ref.rsplit("/", 1)[-1]
            if name in BODY_SAMPLES:
                return {
                    "mimeType": "application/json",
                    "text": json.dumps(BODY_SAMPLES[name], indent=2),
                }
        return {"mimeType": "application/json", "text": "{\n  \n}"}
    return {}


def build_resources(spec: dict) -> list[dict]:
    workspace_id = uid("wrk")
    env_id = uid("env")
    spec_id = uid("spec")

    resources: list[dict] = [
        {
            "_type": "workspace",
            "_id": workspace_id,
            "name": "MantisBase API",
            "description": "MantisBase REST API (`/api/v1/`). Import env vars, run Admin Login, copy token to admin_token.",
            "scope": "collection",
        },
        {
            "_type": "environment",
            "_id": env_id,
            "parentId": workspace_id,
            "name": "Base Environment",
            "data": {
                "base_url": "http://localhost:7070/api/v1",
                "admin_email": "admin@example.com",
                "admin_password": "changeme123",
                "admin_token": "",
                "user_token": "",
                "entity_name": "users",
                "schema_name": "posts",
                "record_id": "",
                "cursor": "",
                "filter": '{"status":"active"}',
                "limit": "50",
                "file_name": "upload.jpg",
                "oauth_provider": "google",
                "oauth_provider_id": "",
                "sse_client_id": "",
                "api_key_id": "",
            },
        },
        {
            "_type": "api_spec",
            "_id": spec_id,
            "parentId": workspace_id,
            "fileName": "openapi.yaml",
            "contentType": "yaml",
            "contents": OPENAPI_PATH.read_text(encoding="utf-8"),
        },
    ]

    tag_folders: dict[str, str] = {}
    for i, tag in enumerate(TAG_ORDER):
        folder_id = uid("fld")
        tag_folders[tag] = folder_id
        resources.append(
            {
                "_type": "request_group",
                "_id": folder_id,
                "parentId": workspace_id,
                "name": tag,
                "description": "",
                "metaSortKey": i,
            }
        )

    sort_key = 0
    paths: dict = spec.get("paths", {})
    for path in sorted(paths.keys()):
        path_item = paths[path]
        for method in ("get", "post", "put", "patch", "delete"):
            if method not in path_item:
                continue
            operation = path_item[method]
            tags = operation.get("tags") or ["System"]
            tag = tags[0] if tags[0] in tag_folders else "System"
            parent_id = tag_folders[tag]

            summary = operation.get("summary") or f"{method.upper()} {path}"
            name = f"{method.upper()} {summary}"

            authentication: dict = {}
            if needs_admin_auth(path, operation, spec):
                authentication = {
                    "type": "bearer",
                    "token": "{{ _.admin_token }}",
                }
            elif needs_user_auth(path, operation):
                authentication = {
                    "type": "bearer",
                    "token": "{{ _.user_token }}",
                }
            elif path == "/auth/verify":
                authentication = {
                    "type": "bearer",
                    "token": "{{ _.user_token }}",
                }

            resources.append(
                {
                    "_type": "request",
                    "_id": uid("req"),
                    "parentId": parent_id,
                    "name": name,
                    "description": operation.get("description", ""),
                    "method": method.upper(),
                    "url": openapi_path_to_url(path),
                    "body": resolve_body(path, method, operation),
                    "parameters": resolve_query_params(operation),
                    "headers": [],
                    "authentication": authentication,
                    "metaSortKey": sort_key,
                }
            )
            sort_key += 1

    return resources


def main() -> None:
    spec = yaml.safe_load(OPENAPI_PATH.read_text(encoding="utf-8"))
    resources = build_resources(spec)
    payload = {
        "_type": "export",
        "__export_format": 4,
        "__export_date": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.000Z"),
        "__export_source": "mantisbase:generate_insomnia.py",
        "resources": resources,
    }
    OUTPUT_PATH.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    request_count = sum(1 for r in resources if r["_type"] == "request")
    print(f"Wrote {OUTPUT_PATH} ({request_count} requests)")


if __name__ == "__main__":
    main()
