# mantisbase (Python)

Python bindings for [mantisbase](https://github.com/allankoechke/mantisbase), a
C++20 single-binary backend-as-a-service. Import it like Flask and drive the
whole server from Python.

```bash
pip install mantisbase
```

```python
import mantisbase

app = mantisbase.App(port=8080, data_dir="./data")


@app.router.get("/hello/{name}")
def hello(req, res):
    res.json(200, {"message": f"Hello, {req.path_param('name')}!"})


@app.router.post("/items")
def create_item(req, res):
    body = req.json()
    rows = app.db.query("INSERT INTO items (name) VALUES (:name) RETURNING *",
                        {"name": body["name"]})
    res.json(201, rows[0])


app.start()  # blocks until stop() is called
```

## API

### `App(**config)`

Keyword arguments mirror the CLI flags: `port`, `host`, `data_dir`, `db_type`,
`db_url`, `public_dir`, `scripts_dir`, `migrations_dir`, `pool_size`, `dev`,
`skip_admin_setup`. `secret_key` sets the `MB_JWT_SECRET` environment variable.

- `start(blocking=True)` — run the HTTP server. Pass `blocking=False` to run it
  on a background thread.
- `stop()` — shut the server down.
- `router` / `db` — the `Router` and `Database` instances.

### `Router`

`get(path, handler=None)`, `post(...)`, `patch(...)`, `delete(...)`. Called with
only a path they return a decorator, so both styles work:

```python
app.router.get("/a", handler)

@app.router.get("/b")
def handler_b(req, res): ...
```

### `MantisRequest`

`path_param(name)`, `query_param(name)`, `header(name)`, `json()`, `body()`,
and the read-only properties `method`, `path`, `remote_addr`.

### `MantisResponse`

`json(status, data)`, `html(status, body)`, `text(status, body)`,
`send(status, data, content_type)`, `redirect(url, status=302)`,
`set_header(name, value)`.

### `Database`

`query(sql, *params)` where each param is a dict of named binds. Returns a list
of dicts keyed by column name. The GIL is released for the duration of the
query.

## Threading

Route handlers are invoked from mantisbase's C++ worker threads, and the GIL is
acquired before each call. Python handlers therefore serialise against each
other — the same model as Flask's default threaded server.

## Building from source

```bash
pip install -e bindings/python
```

Requires CMake >= 3.22, a C++20 compiler, and the mantisbase shared library
(`cmake -DMB_BUILD_SHARED_LIB=ON -DMB_BUILD_BINDINGS=ON ..`).

## License

MIT
