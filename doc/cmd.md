@page cli Command Line Usage

MantisBase has a comprehensive CLI built on `argparse` for server management, admin accounts, migrations, and schema operations.

# mantisbase CLI Reference

```bash
mantisbase [global options] <subcommand> [subcommand options]
```

Only **one subcommand** may be used per invocation: `serve`, `admins`, `migrations`, or `schema`.

---

## Global Options

These options can appear before the subcommand:

| Option | Alias | Description | Default | Env override |
|--------|-------|-------------|---------|--------------|
| `--migrations-dir <path>` | `--migrationsDir` | Migrations directory | `./migrations` | — |
| `--data-dir <path>` | `--dataDir` | Data storage directory | `./data` | — |
| `--public-dir <path>` | `--publicDir` | Static file directory | `./public` | — |
| `--scripts-dir <path>` | `--scriptsDir` | JavaScript extensions directory | `./scripts` | — |
| `--disable-scripting` | `--no-scripting` | Skip JavaScript VM init and script load at runtime | off | `MB_SCRIPTING_DISABLED=1` |
| `--dev` | | Enable verbose development logging | off | `MB_LOG_LEVEL` |
| `--db <type>` | | Database type (`sqlite3`, `postgresql`, `mysql`) | `sqlite3` | `MB_DATABASE_TYPE` |
| `--db_url <url>` | | Database connection string | *(empty)* | `MB_DATABASE_URL` |

When an environment variable is set, it overrides the matching CLI option. For logging, `MB_LOG_LEVEL` (`trace`, `debug`, `info`, `warn`, `critical`) overrides `--dev`.

---

## serve

Start the HTTP server.

```bash
mantisbase serve [--host=<host>] [--port=<port>] [--pool-size=<int>] [--skip-admin-setup]
```

| Option | Description | Default |
|--------|-------------|---------|
| `--host` | Bind address | `0.0.0.0` |
| `--port` | Listen port | `7070` |
| `--pool-size` | Database connection pool size (`--poolSize`) | `4` (sqlite3), `10` (postgresql) |
| `--skip-admin-setup` | Skip first-boot admin browser setup | off |

`MB_SKIP_ADMIN_SETUP=1` also skips admin setup (even without the flag).

| Env variable | Description |
|--------------|-------------|
| `MB_CORS_ORIGINS` | Comma-separated browser origins allowed for cross-origin API requests with credentials. Merged with `corsAllowedOrigins` from app settings. Requires a server restart to pick up changes (unlike PATCH settings). Example: `http://localhost:3000,https://app.example.com` |

See [REST API Reference — CORS](api.md#cross-origin-resource-sharing-cors) for runtime updates via settings PATCH.

**Example:**

```bash
mantisbase serve --host=127.0.0.1 --port=7070
mantisbase --dev serve --port=8000
```

---

## admins

Manage admin accounts. Exactly **one** of `--add`, `--ls`, or `--rm` is required.

```bash
mantisbase admins --add <email> <password>
mantisbase admins --add                    # uses MB_DEFAULT_ADMIN_EMAIL/PASSWORD
mantisbase admins --ls
mantisbase admins --rm <email-or-id>
```

| Option | Description |
|--------|-------------|
| `--add <email> <password>` | Create a new admin user |
| `--ls` | List admin accounts |
| `--rm <identifier>` | Remove admin by email or id |

**Examples:**

```bash
mantisbase admins --add admin@example.com 'secure-password'
mantisbase admins --ls
mantisbase admins --rm admin@example.com
```

---

## schema

Manage entity schemas locally (without the HTTP API). Exactly **one** of `--ls`, `--rm`, `--add`, or `--update` is required.

```bash
mantisbase schema --ls
mantisbase schema --rm <entity>
mantisbase schema --add <json-file-or-string>
mantisbase schema --update <entity> <json-file-or-string>
```

| Option | Description |
|--------|-------------|
| `--ls` | List schemas in a table view |
| `--rm <entity>` | Drop entity schema and table |
| `--add <json>` | Create schema from JSON file or inline JSON |
| `--update <entity> <json>` | Update schema from JSON file or inline JSON |

**Examples:**

```bash
mantisbase schema --ls
mantisbase schema --add ./schemas/posts.json
mantisbase schema --update posts ./schemas/posts-v2.json
mantisbase schema --rm posts
```

---


## migrate

Exactly **one** of `apply` or `schema` is required.

```bash
mantisbase apply [options]
mantisbase schema [options]
```

### apply

Apply or rollback migrations from the migrations directory. Exactly **one** of `--up` or `--down` is required.

```bash
mantisbase migrate apply --up
mantisbase migrate apply  --down
```

| Option | Description |
|--------|-------------|
| `--up` | Apply sorted `*.json` migration files from `--migrations-dir` |
| `--down` | Rollback migrations *(not yet implemented)* |

**Example:**

```bash
mantisbase --migrations-dir=./db/migrations migrate apply --up
```

### schema

Create or load schema dump from a given .json file. Exactly **one** of `--to` or `--from` is required.

```bash
mantisbase migrate schema --to <file.json>
mantisbase migrate apply  --from <file.json>
```

> NB: `file.json` can be an absolute path or a relative path.  


| Option   | Description                                |
|----------|--------------------------------------------|
| `--to`   | Dump current schema to `<file.json>`       |
| `--from` | Load schema from the provided `<file.json>` |

**Example:**

```bash
mantisbase migrate schema --to 123.json
mantisbase migrate schema --from /path/to/123.json
```

---

## See Also

* [Quick Start](QuickStart.md)
* [Embedding MantisBase](embedding.md)
* [REST API Reference](api.md)
* [Authentication API](auth.md)

---
