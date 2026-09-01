# Schema migrations

MantisBase migration files are **entity schema JSON** applied in sorted filename order from a migrations directory.

## How it works

1. Place numbered `*.json` files in a directory (e.g. `001_users.json`, `002_posts.json`).
2. Run `mantisbase --migrations-dir=./migrations migrate apply --up`.
3. Each file is validated and passed to `EntitySchema::createTable`.

> **Note:** `migrate apply --down` (rollback) is not yet implemented.

## Apply bundled migrations

From this directory:

```bash
# Copy migrations to your project, or run from repo examples path
mantisbase --migrations-dir=./examples/migrations migrate apply --up
```

Apply order: `001_users.json` → `002_posts.json`.

## Dump and restore

Use `migrate schema` to snapshot or restore all non-system schemas:

```bash
# Dump current schemas to a file
mantisbase migrate schema --to ./backup-schemas.json

# Restore schemas from dump (creates tables)
mantisbase migrate schema --from ./backup-schemas.json
```

See [`dump-restore.sh`](dump-restore.sh) for a scripted workflow.

## Docker

Mount migrations read-only (see [`../deployment/docker-compose.postgres.yaml`](../deployment/docker-compose.postgres.yaml)):

```yaml
volumes:
  - ./examples/migrations:/mb/migrations:ro
```

## Next steps

- [CLI Reference](../../doc/cmd.md#migrate)
- [Deployment](../deployment/) — production setup with migrations volume
