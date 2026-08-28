# JavaScript scripting

Custom HTTP routes and database queries via Duktape (`index.mantis.js`).

**Requires:** MantisBase built with `MB_SCRIPTING_ENABLED=ON`.

## Setup

1. Copy scripts to your server scripts directory (default: `./scripts` next to the binary):

```bash
mkdir -p ./scripts
cp scripts/index.mantis.js ./scripts/
```

Or with Docker, mount the folder:

```bash
docker run -v $(pwd)/examples/07-scripting/scripts:/mb/scripts:ro ...
```

2. Start server with scripting enabled:

```bash
mantisbase --scriptsDir=./scripts serve
```

## Test custom route

```bash
curl http://localhost:7070/api/v1/custom/health
```

Expected: `{"status":"ok","source":"script"}`

## Next steps

- [Scripting Guide](../../doc/scripting.md) — router, middlewares, request/response API
