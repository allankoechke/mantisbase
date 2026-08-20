@page docker Running in Docker

MantisBase provides Docker support for easy deployment and containerization. This guide covers building and running MantisBase in Docker containers.

---

## Limitations

Docker builds are currently available for `linux-amd64` platform only. For ARM builds, you will need to build the project from source.

---

## Building and Running

### Using Dockerfile

The repository includes a `Dockerfile` in the `/docker` directory. To build and run:

```bash
# Build (optionally set MB_VERSION for a specific release)
docker build -t mantisbase -f docker/Dockerfile --build-arg MB_VERSION=0.4.0 .
docker run -p 7070:8080 --rm mantisbase
```

This builds the image and starts the container, exposing MantisBase on port `8080`. Pass environment variables with `-e`:

```bash
docker run -p 7070:8080 \
  -e MB_JWT_SECRET="$(openssl rand -base64 48)" \
  -v $(pwd)/data:/mb/data \
  -v $(pwd)/public:/mb/public \
  -v $(pwd)/scripts:/mb/scripts \
  --rm mantisbase
```

### Persistent Data

By default, container restarts will clear all data. To persist data, mount volumes to the following directories:

- `/mb/data` - Database files, logs, and uploaded files
- `/mb/public` - Static files to serve
- `/mb/scripts` - JavaScript extension scripts (entry point: `index.mantis.js`)

**Example with volumes:**

```bash
docker run -p 7070:8080 \
  -v $(pwd)/data:/mb/data \
  -v $(pwd)/public:/mb/public \
  -v $(pwd)/scripts:/mb/scripts \
  --rm mantisbase
```

---

## Docker Compose

A `docker-compose.yaml` file is provided for easier management:

```bash
cd docker
# Required: set MB_JWT_SECRET in .env or export before running
# Example: echo "MB_JWT_SECRET=$(openssl rand -base64 48)" > .env
docker compose -f docker-compose.yaml up --build
```

This builds the image (using `MB_VERSION` build arg, default `0.4.0`) and starts the container with volume mounts configured. `MB_JWT_SECRET` **must** be set outside dev mode - the server refuses to start without it. Set it and any other `MB_*` variables in a `.env` file in the same directory, or pass them when running `docker compose`.

---

## Container Hardening

The shipped image and compose file apply a few defaults worth knowing about:

- **Non-root**: the container runs as the unprivileged `mantisbase` user, so it listens on `8080` rather than `80`. Publish it wherever you like (`-p 7070:8080`).
- **Read-only root filesystem**: `docker-compose.yaml` sets `read_only: true` with a `tmpfs` for `/tmp`; only the mounted `./data` volume is writable. `./www`, `./scripts` and `./migrations` are mounted read-only.
- **No privilege escalation**: `security_opt: [no-new-privileges:true]`, plus memory/CPU limits and a file-descriptor `ulimit`.
- **Optional release pinning**: `docker build --build-arg MB_SHA256=<sha256 of the release zip>` verifies the downloaded archive before it is unpacked.
- **Upload limits**: the `maxFileSize` app setting (bytes, default `10485760`) is enforced at upload time, and uploads are restricted to an allow-list of file extensions.

---

## Configuration

You can configure MantisBase using environment variables or by mounting a configuration file. The container respects the same configuration options as the CLI. All environment variables use the **`MB_*`** prefix:

| Variable | Description | Default |
|----------|-------------|---------|
| `MB_JWT_SECRET` | JWT secret key for token signing. **Required in production** (server refuses to start without it when not in dev mode) | (dev mode fallback only) |
| `MB_VERSION` | Build-time only: release version to download (Dockerfile `ARG`) | `0.4.0` |
| `MB_DISABLE_FILE_UPLOADS` | Set to `1` to disable file uploads (returns 403 on upload attempts) | `0` |
| `MB_DISABLE_ADMIN_ON_FIRST_BOOT` | Set to `1` to skip creating admin on first boot | `0` |
| `MB_DISABLE_RATE_LIMIT` | Set to `1` to disable rate limiting. **Only honoured in `--dev` mode**; ignored otherwise | (enabled) |
| `MB_LOG_LEVEL` | Logging verbosity: `trace`, `debug`, `info`, `warn`, `critical` | `info` |
| `MB_DATABASE_TYPE` | Database type: `sqlite3`, `postgresql` | `sqlite3` |
| `MB_DATABASE_URL` | Database connection string (for PostgreSQL) | — |
| `MB_SKIP_ADMIN_SETUP` | Set to `1` to skip the first-boot admin setup prompt | `0` |
| `MB_DEFAULT_ADMIN_EMAIL` | Admin email for `admins --add` when no positional args given | — |
| `MB_DEFAULT_ADMIN_PASSWORD` | Admin password for `admins --add` when no positional args given | — |
| `MB_OAUTH_ENCRYPTION_KEY` | Encryption key for OAuth client secrets. **Required when OAuth is used** (at least 32 chars). No longer falls back to `MB_JWT_SECRET`, so the token-signing key and the secret-at-rest key stay separate | — |
| `MB_TRUSTED_PROXIES` | Comma-separated list of reverse-proxy IPs whose `X-Forwarded-For` header is trusted for client-IP resolution. **If unset, `X-Forwarded-For` is ignored** and the direct peer address is used, so rate limits cannot be bypassed by spoofing the header. Set this when running behind a proxy or load balancer | — |
| `MB_REALTIME_SSE` | Set to `"false"` to disable the SSE realtime endpoint (returns 503) | enabled |
| `MB_REALTIME_WS` | Set to `"false"` to disable the WebSocket realtime endpoint | enabled |
| `MB_CORS_ORIGINS` | Comma-separated browser origins allowed for cross-origin API requests with credentials (merged with `corsAllowedOrigins` from app settings). Example: `http://localhost:3000,https://app.example.com` | — |
| `MB_DISABLE_ADMIN_MUTATIONS` | Set to `true`, `1`, `on`, or `yes` to block admin account create/update/delete via the API (returns **503**). Unset or any other value: edits allowed. | unset (edits allowed) |
| `MB_DISABLE_CONFIG_MUTATIONS` | Set to `true`, `1`, `on`, or `yes` to block `PATCH /api/v1/sys/settings/config` (returns **503**) | unset (edits allowed) |

Database connection is configured via command-line arguments or JSON config (e.g. `--db postgresql --db_url "..."`).

---

## Accessing the API

Once the container is running, access the API at:

- API Endpoints: `http://localhost:7070/api/v1/`
- Admin Dashboard: `http://localhost:7070/mb`
- Health Check: `http://localhost:7070/api/v1/health`

---

## Example: Full Setup with PostgreSQL

```yaml
services:
  mantisbase:
    build:
      context: .
      dockerfile: docker/Dockerfile
      args:
        MB_VERSION: "0.4.0"
    image: mantisbase:latest
    restart: unless-stopped
    ports:
      - "7070:8080"
    volumes:
      - ./data:/mb/data
      - ./public:/mb/public
      - ./scripts:/mb/scripts
    environment:
      MB_JWT_SECRET: "${MB_JWT_SECRET:?Set MB_JWT_SECRET via .env or environment}"
    command: ["--db", "postgresql", "--db_url", "dbname=mantis host=postgres port=5432 user=mb_app password=${POSTGRES_PASSWORD}", "serve", "--port", "8080"]
    depends_on:
      - postgres

  postgres:
    image: postgres:15
    environment:
      POSTGRES_DB: mantis
      POSTGRES_USER: mb_app
      POSTGRES_PASSWORD: "${POSTGRES_PASSWORD:?Set POSTGRES_PASSWORD via .env or environment}"
    volumes:
      - postgres_data:/var/lib/postgresql/data

volumes:
  postgres_data:
```