# MantisBase — Agent Guide

MantisBase is a self-hosted C++20 BaaS. Define JSON entity schemas and get instant REST CRUD, JWT auth, SSE realtime, file uploads, and an admin dashboard at `/mb`.

## Start here (vibecoding)

1. Browse copy-paste examples: [`examples/README.md`](examples/README.md)
2. Run `./mantisbase serve` (or Docker)
3. Create admin at `http://localhost:7070/mb`
4. Apply a schema from `examples/` and test with curl scripts

## Cursor skills

Project skills in `.cursor/skills/` guide AI-assisted backend building:

| Skill | Use for |
|-------|---------|
| `mantisbase-vibecode` | End-to-end workflow |
| `mantisbase-schemas` | Entity schemas, FK, migrations |
| `mantisbase-auth` | Login, tokens, API keys |
| `mantisbase-access-rules` | Permissions and 401/403 debugging |
| `mantisbase-realtime-files` | SSE and file uploads |

## Key paths

- API base: `/api/v1/`
- Schemas: `POST /api/v1/schemas` (admin)
- Entities: `/api/v1/entities/{name}`
- Auth: `/api/v1/auth/{entity}/login`

## Documentation

- [Quick Start](doc/QuickStart.md)
- [API Reference](doc/api.md)
- [Authentication](doc/auth.md)
- [Access Rules](doc/rules.md)

For C++ core development, see [CONTRIBUTING.md](CONTRIBUTING.md).
