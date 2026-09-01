# Production deployment

Docker Compose and environment templates for running MantisBase with PostgreSQL and persistent volumes.

## Quick start (PostgreSQL)

1. Copy and edit environment:

```bash
cp env.production.example .env
# Set MB_JWT_SECRET to a strong random value:
# openssl rand -base64 48
```

2. Start stack:

```bash
docker compose -f docker-compose.postgres.yaml up -d
```

API: `http://localhost:7070/api/v1/` — Admin: `http://localhost:7070/mb`

## Scaling notes

| Topic | Guidance |
|-------|----------|
| **SQLite → PostgreSQL** | Export schemas with `migrate schema --to`, point new instance at PostgreSQL via `MB_DB_URL`, restore with `--from`. Migrate data separately (SQL dump or custom script). |
| **Connection pool** | PostgreSQL default pool size is 10 (`app.poolSize`). Increase for high concurrency; monitor DB connections. |
| **Multiple instances** | Run separate containers behind a reverse proxy. Use shared PostgreSQL and shared `data` volume for files, or object storage for uploads. |
| **Health checks** | `GET /api/v1/health` — use for load balancer probes ([healthcheck doc](../../doc/healthcheck.md)). |
| **Secrets** | Always set `MB_JWT_SECRET` in production. Never commit secrets to git. |
| **File uploads** | Mount `/mb/data` persistently. Set `MB_DISABLE_FILE_UPLOADS=1` if uploads are not needed. |

## Resource limits

The compose file sets memory and CPU limits suitable for small deployments. Adjust `deploy.resources` for your workload.

## Next steps

- [Docker Guide](../../doc/docker.md)
- [Migrations](../migrations/) — schema-only bootstrap
- [Health Check](../../doc/healthcheck.md)
