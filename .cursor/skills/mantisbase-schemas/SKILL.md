---
name: mantisbase-schemas
description: >-
  Define MantisBase entity schemas, field types, validators, and foreign keys.
  Use when creating tables, migrations, or CRUD APIs via JSON schema.
---

# MantisBase Schemas

## Schema skeleton

```json
{
  "name": "my_entity",
  "type": "base",
  "fields": [
    {"name": "title", "type": "string", "required": true}
  ],
  "rules": {
    "list": {"mode": "public", "expr": ""},
    "get": {"mode": "public", "expr": ""},
    "add": {"mode": "auth", "expr": ""},
    "update": {"mode": "auth", "expr": ""},
    "delete": {"mode": "", "expr": ""}
  }
}
```

## Entity types

| type | Purpose |
|------|---------|
| `base` | Standard CRUD entity |
| `auth` | User accounts with login (`email`, `password` fields) |
| `view` | Read-only SQL view (advanced) |

## Common field types

`string`, `int`, `double`, `bool`, `date`, `json`, `file`, `files`, `relation`

## Validators (constraints)

```json
{"name": "email", "type": "string", "constraints": {"validator": "@email"}}
{"name": "password", "type": "string", "constraints": {"validator": "@password"}}
```

## Foreign keys

Reference must exist before dependent schema (or expect DB enforcement):

```json
{
  "name": "user_id",
  "type": "string",
  "foreign_key": {
    "entity": "users",
    "field": "id",
    "on_delete": "CASCADE"
  }
}
```

See [`examples/03-ecommerce/schemas/`](../../examples/03-ecommerce/schemas/).

## Apply schema

**API:** `POST /api/v1/schemas` with admin Bearer token.

**CLI migration:** numbered `*.json` in migrations dir → `mantisbase migrate apply --up`.

Migration JSON = same schema format. Rollback (`--down`) not implemented.

## Examples

- Blog: [`examples/01-blog/schemas/posts.json`](../../examples/01-blog/schemas/posts.json)
- Auth users: [`examples/02-auth-users/schemas/users.json`](../../examples/02-auth-users/schemas/users.json)
- Migrations: [`examples/migrations/`](../../examples/migrations/)

## Docs

- [API Reference — Schemas](../../doc/api.md)
- [CLI migrate](../../doc/cmd.md#migrate)
