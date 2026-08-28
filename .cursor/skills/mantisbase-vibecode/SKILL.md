---
name: mantisbase-vibecode
description: >-
  End-to-end workflow for vibecoding apps on MantisBase BaaS. Use when building
  backends with schemas, auth, CRUD, rules, or realtime without writing C++.
---

# MantisBase Vibecoding

MantisBase is a self-hosted C++ BaaS: define JSON schemas → get instant REST APIs, JWT auth, SSE realtime, and file uploads.

## Workflow

1. **Pick a starting example** from [`examples/`](../../examples/README.md):
   - Blog CRUD → `examples/01-blog/`
   - Auth users → `examples/02-auth-users/`
   - E-commerce FK → `examples/03-ecommerce/`
2. **Start server**: `./mantisbase serve` or Docker (`doc/docker.md`).
3. **Create admin** at `/mb` or `./mantisbase admins --add admin@example.com <password>`.
4. **Apply schema** via API or migration:
   ```bash
   curl -X POST http://localhost:7070/api/v1/schemas \
     -H "Authorization: Bearer $ADMIN_TOKEN" \
     -H "Content-Type: application/json" \
     -d @examples/01-blog/schemas/posts.json
   ```
5. **Test with curl** using scripts in `examples/*/http/`.
6. **Iterate** — patch schema or rules, add entities, wire frontend to `/api/v1/`.

## Key API paths

| Purpose | Endpoint |
|---------|----------|
| Create schema | `POST /api/v1/schemas` (admin) |
| CRUD | `/api/v1/entities/{name}` |
| Login | `POST /api/v1/auth/{entity}/login` |
| Verify | `GET /api/v1/auth/verify` |
| Realtime SSE | `GET /api/v1/realtime?topics={entity}` |
| Files | `POST /api/v1/entities/{name}` (multipart) |

## Safety defaults

- Never hardcode `MB_JWT_SECRET` — use env vars in production.
- Default writes to `auth` mode unless explicitly public.
- Schema changes require admin token.
- Link to docs; don't duplicate full API reference.

## Related skills

- `mantisbase-schemas` — entity/field design
- `mantisbase-auth` — login, tokens, API keys
- `mantisbase-access-rules` — permissions
- `mantisbase-realtime-files` — SSE and uploads

## Docs

- [Quick Start](../../doc/QuickStart.md)
- [API Reference](../../doc/api.md)
