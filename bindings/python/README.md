# mantisbase (Python)

Python bindings for [MantisBase](https://github.com/allankoechke/mantisbase), a C++20 backend-as-a-service. Import it and drive the server, database, auth, files, and realtime API from Python.

```bash
pip install mantisbase
```

Pre-release tags are published with a PEP 440 marker: `v0.4.9-alpha.1` is `0.4.9a1`, `v0.4.9-beta.1` is `0.4.9b1`, and `v0.4.9-rc.1` is `0.4.9rc1`. Install one with `pip install mantisbase==0.4.9b1`. `pip install mantisbase` still selects the newest stable release.

Full API reference: <https://allankoechke.github.io/mantisbase/docs_python.html>

Source for that page: <https://github.com/allankoechke/mantisbase/blob/master/doc/python.md>

```python
import mantisbase

app = mantisbase.App(port=8080, data_dir="./data", dev=True)

app.create_schema({
    "name": "notes",
    "type": "base",
    "fields": [{"name": "title", "type": "string", "required": True}],
})


@app.router.get("/notes/{id}")
def read_note(req, res):
    note = app.entity("notes").read(req.path_param("id"))
    if note is None:
        res.json(404, {"error": "Note not found"})
        return
    res.json(200, note)


app.start()
```

Pass `secret_key` (or set `MB_JWT_SECRET`) before `start()` in production. Without a signing key, and without `dev=True`, the process exits.

The bindings cover routing and middleware, request and response helpers, SQL queries and transactions, entity CRUD, schema management, JWT sessions, API keys, OAuth, files, settings, logs, realtime fan-out, mail, and the small `mantisbase.utils` helpers. Method names and arguments are listed in the [Python bindings guide](https://allankoechke.github.io/mantisbase/docs_python.html).

## Threading

Route handlers run on MantisBase worker threads. The GIL is held around Python callbacks and released during database and filesystem work.

## Building from source

Build and install the shared library, then point the bindings at that prefix:

```bash
cmake -B build -DMB_BUILD_SHARED_LIB=ON
cmake --build build
cmake --install build --prefix ./install

pip install -e bindings/python \
  --config-settings=cmake.define.CMAKE_PREFIX_PATH=$PWD/install
```

Requires CMake >= 3.22, a C++20 compiler, and Python >= 3.9. `nanobind` and `scikit-build-core` are fetched by pip. Export `LD_LIBRARY_PATH=$PWD/install/lib` so the loader can find `libmantisbase.so`.

Wheel production is described in `.github/workflows/bindings-python.yml`.

## Tests

With a wheel installed in the active environment, from the repository root:

```bash
python -m unittest discover -s bindings/python/tests -v
```

The suite starts a temporary SQLite server and checks schemas, entity CRUD, SQL transactions, JWT and API keys, files, the mock mailer, and HTTP routes. CI runs it against every repaired wheel.

## License

MIT
