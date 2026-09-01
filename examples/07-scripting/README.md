# JavaScript scripting

Custom HTTP routes and database queries via Duktape (`main.mb.js`).

**Requires:** MantisBase built with `MB_SCRIPTING_ENABLED=ON`.

## Setup

1. Copy scripts to your server scripts directory (default: `./scripts` next to the binary):

```bash
mkdir -p ./scripts
cp examples/07-scripting/scripts/main.mb.js ./scripts/
```

Or with Docker, mount the folder:

```bash
docker run -v $(pwd)/examples/07-scripting/scripts:/mb/scripts:ro ...
```

2. Start server with scripting enabled:

```bash
mantisbase --scriptsDir=./scripts serve
```

To disable scripting at runtime (even when compiled in):

```bash
mantisbase --disable-scripting serve
# or: MB_SCRIPTING_DISABLED=1 mantisbase serve
```

## Test custom route

```bash
curl http://localhost:7070/api/v1/custom/health
```

Expected: `{"status":"ok","source":"script"}`

## Entry script

| File | Status |
|---|---|
| `main.mb.js` | Primary entry point |
| `index.mantis.js` | Deprecated fallback (logged once at startup) |

## Next steps

- [Scripting Guide](../../doc/scripting.md) — router, middlewares, request/response API
- [Example scripts](./scripts/main.mb.js)
