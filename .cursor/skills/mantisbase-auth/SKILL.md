---
name: mantisbase-auth
description: >-
  MantisBase authentication: auth entities, JWT login/refresh/logout, cookies,
  API keys. Use when adding user accounts or securing API calls.
---

# MantisBase Auth

## Auth entity schema

```json
{
  "name": "users",
  "type": "auth",
  "fields": [
    {"name": "name", "type": "string", "required": true},
    {"name": "email", "type": "string", "required": true, "unique": true},
    {"name": "password", "type": "string", "required": true}
  ],
  "rules": {
    "add": {"mode": "public", "expr": ""},
    "list": {"mode": "auth", "expr": ""},
    "get": {"mode": "auth", "expr": ""},
    "update": {"mode": "auth", "expr": ""},
    "delete": {"mode": "", "expr": ""}
  }
}
```

Example: [`examples/02-auth-users/`](../../examples/02-auth-users/).

## Endpoints

| Action | Method | Path |
|--------|--------|------|
| Register | POST | `/api/v1/entities/{entity}` |
| Login | POST | `/api/v1/auth/{entity}/login` |
| Refresh | POST | `/api/v1/auth/{entity}/refresh` |
| Logout | POST | `/api/v1/auth/{entity}/logout` |
| Verify | GET | `/api/v1/auth/verify` |
| Admin login | POST | `/api/v1/sys/admins/login` |

Login body: `{"identity": "user@example.com", "password": "..."}`

Response: `data.token` (JWT).

## Using tokens

**Bearer header:**
```
Authorization: Bearer <token>
```

**Cookie:** Login sets HttpOnly `mb_token` — usable on `/api/v1/auth/verify` without Bearer.

## API keys

Machine-to-machine auth via admin sys routes. See [auth.md — API Keys](../../doc/auth.md).

## Security

- Set `MB_JWT_SECRET` in production (never commit secrets).
- Login rate-limited: 5 attempts/minute per IP.
- Invalid login → 404; invalid token → 401.

## Test script

[`examples/02-auth-users/http/auth-flow.sh`](../../examples/02-auth-users/http/auth-flow.sh)

## Docs

- [Authentication API](../../doc/auth.md)
