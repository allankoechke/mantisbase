# HTTP API

- `GET /api/v1/health` — liveness
- `GET /api/v1/openapi.json` — OpenAPI 3.0 document (includes catalog snapshot)
- `GET|POST /api/v1/admins` — list / create admin accounts (admin Basic)
- `DELETE /api/v1/admins/{id}` — remove admin by row id or email (admin Basic)
- `GET|POST /api/v1/sys/schemas` — list / create entity schemas (admin)
- `GET|DELETE /api/v1/sys/schemas/{name}` — fetch / drop schema (admin)
- `GET|POST /api/v1/entities/{entity}` — list / create rows (access rules)
- `GET|PATCH|DELETE /api/v1/entities/{entity}/{id}` — row CRUD
- `POST /api/v1/auth/login` — application user login (JWT)
- `GET|PATCH /api/v1/sys/configs` — application configuration (admin)
- `GET /api/v1/sys/logs` — system logs (admin; structured response, tail integration TBD)
- `GET|POST /api/v1/webhooks` — list / register webhooks (admin)

The compiled admin SPA is served under `/mb/` from `./public/mb-dist` by default (`cd admin && npm run build`; override with `--admin-ui-dir` or `MB_ADMIN_UI_DIR`).
