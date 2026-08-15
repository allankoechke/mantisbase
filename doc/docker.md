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
docker build -t mantisbase -f docker/Dockerfile --build-arg MB_VERSION=0.3.4 .
docker run -p 7070:80 --rm mantisbase
```

This builds the image and starts the container, exposing MantisBase on port `80`. Pass environment variables with `-e`:

```bash
docker run -p 7070:80 \
  -e MB_JWT_SECRET=your-secret-key-here \
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
docker run -p 7070:80 \
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
# Optional: set MB_JWT_SECRET and MB_VERSION in .env or export before running
docker compose -f docker-compose.yaml up --build
```

This builds the image (using `MB_VERSION` build arg, default `0.3.4`) and starts the container with volume mounts configured. Set `MB_JWT_SECRET` and other `MB_*` variables in a `.env` file in the same directory or pass them when running `docker compose`.

---

## Configuration

You can configure MantisBase using environment variables or by mounting a configuration file. The container respects the same configuration options as the CLI. All environment variables use the **`MB_*`** prefix:

| Variable | Description | Default |
|----------|-------------|---------|
| `MB_JWT_SECRET` | JWT secret key for token signing. **Required in production** (server refuses to start without it when not in dev mode) | (dev mode fallback only) |
| `MB_VERSION` | Build-time only: release version to download (Dockerfile `ARG`) | `0.3.4` |
| `MB_DISABLE_FILE_UPLOADS` | Set to `1` to disable file uploads (returns 403 on upload attempts) | `0` |
| `MB_DISABLE_ADMIN_ON_FIRST_BOOT` | Set to `1` to skip creating admin on first boot | `0` |
| `MB_DISABLE_RATE_LIMIT` | Set to `1` to disable rate limiting | (enabled) |
| `MB_LOG_LEVEL` | Logging verbosity: `trace`, `debug`, `info`, `warn`, `critical` | `info` |
| `MB_DATABASE_TYPE` | Database type: `sqlite3`, `postgresql` | `sqlite3` |
| `MB_DATABASE_URL` | Database connection string (for PostgreSQL) | — |
| `MB_SKIP_ADMIN_SETUP` | Set to `1` to skip the first-boot admin setup prompt | `0` |
| `MB_DEFAULT_ADMIN_EMAIL` | Admin email for `admins --add` when no positional args given | — |
| `MB_DEFAULT_ADMIN_PASSWORD` | Admin password for `admins --add` when no positional args given | — |
| `MB_OAUTH_ENCRYPTION_KEY` | Encryption key for OAuth client secrets (32 chars; falls back to `MB_JWT_SECRET` padded to 32) | — |
| `MB_REALTIME_SSE` | Set to `"false"` to disable the SSE realtime endpoint (returns 503) | enabled |
| `MB_REALTIME_WS` | Set to `"false"` to disable the WebSocket realtime endpoint | enabled |
| `MB_DISABLE_ADMIN_EDITS` | Set to `true` to disable admin accounts editing for production systems | disabled |

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
        MB_VERSION: "0.3.4"
    image: mantisbase:latest
    restart: unless-stopped
    ports:
      - "7070:80"
    volumes:
      - ./data:/mb/data
      - ./public:/mb/public
      - ./scripts:/mb/scripts
    environment:
      MB_JWT_SECRET: "your-secret-key"
    command: ["--db", "postgresql", "--db_url", "dbname=mantis host=postgres port=5432 user=postgres password=postgres", "serve", "--port", "80"]
    depends_on:
      - postgres

  postgres:
    image: postgres:15
    environment:
      POSTGRES_DB: mantis
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
    volumes:
      - postgres_data:/var/lib/postgresql/data

volumes:
  postgres_data:
```