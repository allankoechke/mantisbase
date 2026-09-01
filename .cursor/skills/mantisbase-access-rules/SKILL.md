---
name: mantisbase-access-rules
description: >-
  MantisBase access rules: public, auth, custom expressions, admin-only, entity
  filters. Use when securing CRUD operations or debugging 401/403 responses.
---

# MantisBase Access Rules

Each entity has five rules: `list`, `get`, `add`, `update`, `delete`.

## Modes

| mode | Behavior |
|------|----------|
| `"public"` | No auth required |
| `"auth"` | Any authenticated user; optional `entity` filter |
| `"custom"` | Evaluate `expr` JavaScript expression |
| `""` (empty) | Admin only |

## Evaluation order

1. Admin short-circuit (`auth.type == "admin"`) → allow
2. `public` → allow
3. Empty mode → admin only
4. `auth` → check token + optional entity filter
5. `custom` → evaluate expression

## Auth entity filter

```json
{"mode": "auth", "entity": "editors"}
{"mode": "auth", "entity": "users,editors"}
{"mode": "auth", "entity": "!guests"}
```

## Custom expressions

Common patterns:

```javascript
auth.id == req.body.author_id
auth.id != "" && auth.id != null
(auth.entity == "mb_admins") || (auth.id == req.body.owner_id)
```

Available in expressions: `auth.id`, `auth.entity`, `auth.user.*`, `req.body.*`

## Examples

- Owner-only update: [`examples/04-access-rules/schemas/posts-owner-only.json`](../../examples/04-access-rules/schemas/posts-owner-only.json)
- Entity filter: [`examples/04-access-rules/schemas/multi-role-users.json`](../../examples/04-access-rules/schemas/multi-role-users.json)

## Debugging status codes

| Code | Typical cause |
|------|---------------|
| 401 | Missing/invalid/expired token |
| 403 | Authenticated but rule denied (custom expr false) |
| 404 | Wrong auth entity name on login route |

Prefer `auth` + `entity` over `custom` when only filtering by auth table.

## Docs

- [Access Rules](../../doc/rules.md)
