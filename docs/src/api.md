# HTTP API

- `GET /api/v1/health` — liveness
- `GET /api/v1/openapi.json` — OpenAPI 3.0 document (includes catalog snapshot)
- `GET|POST /api/v1/schemas` — list / create entity schemas (admin)
- `GET|DELETE /api/v1/schemas/{name}` — fetch / drop schema (admin)
- `GET|POST /api/v1/entities/{entity}` — list / create rows (access rules)
- `GET|PATCH|DELETE /api/v1/entities/{entity}/{id}` — row CRUD
- `POST /api/v1/auth/login` — auth-entity login (JWT)
- `GET|PATCH /api/v1/config` — application configuration (admin)
- `GET|POST /api/v1/webhooks` — list / register webhooks (admin)

Static admin assets are served from `/mb` (defaults to `./public`).
