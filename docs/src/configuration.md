# Configuration

Bootstrap settings use environment variables with the `MB_` prefix, for example:

- `MB_DATABASE_URL` — optional override for non-local database URLs
- `MB_JWT_SECRET` — required to issue user JWTs from `/api/v1/auth/login`
- `MB_TURSO_AUTH_TOKEN` — required when using `--db turso`
- `MB_LOG_LEVEL` — `trace` … `critical`
- `MB_ADMIN_UI_DIR` — directory containing the built admin `index.html` (default `./public/mb-dist`; build with `cd admin && npm run build`)
- `MB_SKIP_ADMIN_SETUP` — if truthy (`1`, `true`, `yes`, `on`), skip first-admin setup on `serve` (same as `--skip-setup`)

Runtime product settings belong in the `mb_app_config` table and are exposed via `GET/PATCH /api/v1/sys/configs`
(admin Basic auth).
