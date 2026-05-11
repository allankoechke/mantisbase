> [!IMPORTANT]
> **MantisBase is being reworked in Rust.** This repository is the active rewrite. Behaviour and APIs may differ from earlier C++ releases, and **feature parity is not guaranteed yet**—some capabilities described in older marketing or in `doc/` may be planned, partial, or unchanged only on legacy binaries. Treat this notice as authoritative until it is removed.

<p align="center">
  <img src="assets/mantisbase-banner.jpg" alt="MantisBase Cover" width="100%" />
</p>

<p align="center">
  <strong>A lightweight Backend-as-a-Service (BaaS) in Rust — single binary, schemas, REST, and an admin UI</strong><br />
  Local <strong>libSQL</strong> by default; optional <strong>Turso</strong> and <strong>PostgreSQL</strong> (<code>postgres</code> feature).
</p>

---

## What is MantisBase?

MantisBase is a small backend you run yourself: define **entity schemas**, get **JSON REST** routes, enforce **access rules**, serve the **admin dashboard** under `/mb`, and optionally register **webhooks**. **Admin accounts** live in a dedicated table; **application users** (JWT login) use a single central **`mb_user`** table—not a special entity type in the catalog.

The rewrite targets the same product goals as the original C++ stack (speed, embeddability, clarity) with a smaller runtime surface while features land incrementally.

---

## Quick start (this tree — Rust)

### Prerequisites

- [Rust](https://rustup.rs/) (see `rust-version` in `Cargo.toml`)
- Optional: PostgreSQL client libs if you build with `--features postgres`

### Build and run

```bash
git clone https://github.com/allankoechke/mantisbase.git
cd mantisbase
cargo build --release
```

Apply migrations, create an admin, set a JWT secret for app-user login, then serve:

```bash
./target/release/mantisbase migrations up
./target/release/mantisbase admins add admin@example.com 'your-admin-password'
export MB_JWT_SECRET='a-long-random-secret-for-user-jwt'
./target/release/mantisbase serve
```

Defaults:

- API: `http://127.0.0.1:7070/api/v1/`
- Admin static UI: `http://127.0.0.1:7070/mb` (use **HTTP Basic** with the admin email and password you created)

OpenAPI for the running shape of the API: `GET /api/v1/openapi.json`.

### Create a schema (admin Basic auth)

Admin routes expect `Authorization: Basic …` (email:password), not a bearer “admin token”.

```bash
curl -sS -u 'admin@example.com:your-admin-password' \
  -X POST http://127.0.0.1:7070/api/v1/schemas \
  -H 'Content-Type: application/json' \
  -d '{
    "name": "posts",
    "type": "base",
    "fields": [
      {"name": "title", "type": "string", "required": true},
      {"name": "content", "type": "string"}
    ],
    "rules": {
      "list": {"mode": "public", "expr": ""},
      "get": {"mode": "public", "expr": ""},
      "add": {"mode": "auth", "expr": ""},
      "update": {"mode": "auth", "expr": ""},
      "delete": {"mode": "", "expr": ""}
    }
  }'
```

**Entity types** (catalog): **`bare`**, **`base`**, **`view`** — there is no `auth` entity type; end-user sign-in is via **`POST /api/v1/auth/login`** against `mb_user`.

### Application user login

```bash
curl -sS -X POST http://127.0.0.1:7070/api/v1/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"email":"user@example.com","password":"user-password"}'
```

Response includes `token` (JWT) and `user` JSON. Use `Authorization: Bearer <token>` on routes whose rules require authenticated access.

---

## What is implemented vs legacy docs

Rough map for **this Rust codebase** (check `src/http/mod.rs` and OpenAPI for truth):

| Area | Status |
|------|--------|
| Health, OpenAPI, CORS, request logging | Implemented |
| Schemas CRUD, entity row CRUD, access rules | Implemented |
| Admin users CLI + Basic auth for admin API | Implemented |
| `mb_user` + `POST /api/v1/auth/login` + JWT | Implemented |
| Config patch, webhooks | Implemented |
| Static `/mb` admin bundle | Served from `public/` |
| SSE realtime, file pipeline, refresh/logout auth routes, Duktape scripting, … | **Not** promised here; older `doc/*.md` may still describe the C++ server |

When in doubt, prefer **`openapi.json`** and this README over historical prose.

---

## CLI overview

| Command | Purpose |
|---------|---------|
| `mantisbase serve` | HTTP server (`--host`, `--port`) |
| `mantisbase admins …` | Add, list, or remove admin users |
| `mantisbase migrations …` | Apply or reset SQL migrations |

Global flags include `--data-dir`, `--public-dir`, `--dev`, `--db` (`libsql` \| `turso` \| `postgresql`), and `--db-url` (or `MB_DATABASE_URL`). Turso typically needs `MB_TURSO_AUTH_TOKEN`.

---

## Configuration (environment)

| Variable | Role |
|----------|------|
| `MB_JWT_SECRET` | Required to issue JWTs from `/api/v1/auth/login` |
| `MB_DATABASE_URL` | DB URL when using Turso/Postgres (or pass `--db-url`) |
| `MB_TURSO_AUTH_TOKEN` | Turso auth token when `--db turso` |
| `MB_LOG_LEVEL` | `trace` … `critical` |

---

## Project layout (Rust)

```
mantisbase/
├── src/              # Library + `mantisbase` binary (CLI, HTTP, storage)
├── migrations/       # libSQL / Turso SQL migrations
├── migrations/postgres/
├── public/           # Admin SPA assets for `/mb`
├── tests/
└── doc/              # Legacy / long-form guides (may predate the Rust port)
```

---

## Optional Cargo features

- **`postgres`** — PostgreSQL backend via sqlx migrations under `migrations/postgres/`
- **`smtp`** — email (lettre)
- **`embed-js`** — experimental QuickJS embedding

Example: `cargo build --release --features postgres`

---

## Documentation and status

- **API:** `GET http://<host>:<port>/api/v1/openapi.json`
- **Hosted book / site:** may track releases; verify against this branch.
- **Contributing:** [CONTRIBUTING.md](CONTRIBUTING.md)

MantisBase remains under active development; breaking changes are possible while the Rust port catches up to the full prior surface.

---

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License

MIT License © 2026 Allan K. Koech

---

## Resources

- [GitHub repository](https://github.com/allankoechke/mantisbase)
