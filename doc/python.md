@page docs_python Python Bindings

Python bindings embed the MantisBase server in a process and expose the same services the C++ API does: routing, SQL, entity CRUD, schemas, auth, files, settings, logs, realtime, and mail.

```bash
pip install mantisbase
```

The published copy of this page is [docs_python.html](https://allankoechke.github.io/mantisbase/docs_python.html).

`start()` refuses to serve in production when `MB_JWT_SECRET` is empty: the process exits. For local use pass `dev=True`. For anything else pass `secret_key` (or export `MB_JWT_SECRET` before creating the app).

```python
import mantisbase

app = mantisbase.App(port=8080, data_dir="./data", dev=True)

app.create_schema({
    "name": "notes",
    "type": "base",
    "fields": [
        {"name": "title", "type": "string", "required": True},
        {"name": "body", "type": "string"},
    ],
    "rules": {
        "list": {"mode": "public"},
        "get": {"mode": "public"},
        "add": {"mode": "auth"},
        "update": {"mode": "auth"},
        "delete": {"mode": ""},
    },
})


@app.router.get("/notes/{id}")
def read_note(req, res):
    note = app.entity("notes").read(req.path_param("id"))
    if note is None:
        res.json(404, {"error": "Note not found"})
        return
    res.json(200, note)


@app.router.post("/notes", middlewares=[mantisbase.middlewares.get_auth_token()])
def create_note(req, res):
    created = app.entity("notes").create(req.json())
    res.json(201, created)


app.start()
```

Handler arguments are `MbRequest` and `MbResponse`. `Router`, `MantisRequest`, `MantisResponse`, and `Database` are aliases of `MbRouter`, `MbRequest`, `MbResponse`, and `MbDatabase`.

Failures from the C++ layer raise `mantisbase.MantisError` (a subclass of `Exception`). `code` is the HTTP-style status and `description` is the longer detail, which may be empty.

```python
try:
    app.entity("nope")
except mantisbase.MantisError as exc:
    print(exc.code, exc)
```

## App

```python
app = mantisbase.App(
    port=8080,
    host="127.0.0.1",
    data_dir="./data",
    db_type="sqlite3",          # or "postgresql"
    db_url="",
    public_dir="./public",
    scripts_dir="./scripts",
    templates_dir="./templates",
    migrations_dir="./migrations",
    pool_size=2,
    dev=True,
    skip_admin_setup=False,
    secret_key="replace-me",    # exported as MB_JWT_SECRET before startup
)
```

| Member | Meaning |
|---|---|
| `start(blocking=True)` | Run the HTTP server. `blocking=False` runs it on a background thread. |
| `stop()` / `close()` | Stop the server and join the background thread. |
| `quit(code=0, reason=...)` | Stop the server, then terminate the process with `std::exit`. |
| `entity(name)` | `Entity` for a table in the schema cache. A missing table or an invalid name raises `MantisError`. |
| `has_entity(name)` | Whether that table is in the schema cache. |
| `create_schema(schema)` | Create a table from a schema dict (same shape as `POST /api/v1/schemas`). |
| `list_schemas(opts=None)` | Schema rows. `opts` matches the C++ list options. |
| `get_schema(table_id)` | One schema by id. |
| `update_schema(table_id, schema)` | Patch a schema. |
| `drop_schema(table_id)` | Drop a table. |
| `table_exists(name)` | Whether the table is stored. |
| `validate_schema(schema)` | `None` when the schema is valid, otherwise the error string. |
| `snowflake_id()` | Next Snowflake id string. |
| `open_admin_dashboard()` | Open the admin UI in the default browser. |
| `router`, `db`, `settings`, `auth`, `files`, `logs`, `rt`, `mailer` | Service objects. Holding one keeps the `App` alive. |
| `host`, `port`, `pool_size` | Read/write. Changes after `start()` do not rebind the listener or the pool. |
| `public_dir`, `data_dir`, `scripts_dir`, `templates_dir`, `migrations_dir` | Read/write paths. |
| `skip_admin_setup` | Read/write. |
| `db_type`, `dev_mode`, `scripting_disabled`, `secret_key`, `version` | Read-only. |
| `uptime_seconds` | Seconds since the app object was created. |

`mantisbase.version()` returns the same version string.

Schema JSON matches the REST schema body. See the [API Reference](api.md) for field types, rules, and indexes.

## Router

Paths use `{param}` placeholders. Pass a handler, or omit it and use the method as a decorator. `middlewares` is a callable, a `Middleware`, or a list of either.

A handler may take `(req, res)`, `(req)`, or no arguments. A return value other than `None` is sent as JSON with status 200. Call `res.json(status, data)` when the status is not 200, or when the handler writes the response itself; that return should stay `None`.

A Python middleware is `(req, res) -> bool`. Return `True` or `None` to continue. Return `False` after writing the response to stop the chain. An exception becomes HTTP 500 `{"error": "..."}` and stops the chain.

| Method | Purpose |
|---|---|
| `get` / `post` / `put` / `patch` / `delete` | Register one verb. |
| `add_route(method, path, handler=None, middlewares=None)` | Same, with the verb as a string (`GET`, `POST`, `PUT`, `PATCH`, `DELETE`). |
| `redirect(source, destination, status=302, methods=None)` | Redirect a path with no handler. `methods` defaults to `["GET"]`. Pass `[]` to allow every method. |
| `broadcast(event)` | Push a JSON-compatible event to SSE and WebSocket subscribers. |
| `is_running` | Whether the listener is up. |
| `sse` | `Sse` manager. |

```python
@app.router.get("/hello")
def hello():
    return {"message": "Hello, World!"}

@app.router.put("/notes/{id}", middlewares=[
    mantisbase.middlewares.get_auth_token(),
    mantisbase.middlewares.require_admin(),
])
def replace_note(req, res):
    updated = app.entity("notes").update(req.path_param("id"), req.json())
    res.json(200, updated)

app.router.redirect("/old", "/notes/1", status=301, methods=["GET", "POST"])
```

`req` and `res` are only valid for the duration of the handler or middleware that received them.

### middlewares

`mantisbase.middlewares` builds the C++ middleware used by the HTTP API:

| Factory | Effect |
|---|---|
| `get_auth_token()` | Read `Authorization: Bearer` or the `mb_token` cookie onto the request. |
| `hydrate_context()` | Attach request metadata for later rules. |
| `resolve_schema()` | Resolve `:schema_name_or_id`. |
| `resolve_entity()` / `resolve_auth_entity()` | Resolve `:entity_name` to a normal or auth entity. |
| `reject_view_mutations()` | Block writes to view entities. |
| `has_entity_access()` / `has_access(entity)` | Enforce the entity access rule. |
| `require_guest()` | Allow only unauthenticated requests. |
| `require_admin()` | Allow only an admin session. |
| `require_entity(entity)` | Allow only that auth entity. |
| `require_admin_or_entity(entity)` | Allow an admin or that entity. |
| `require_expr(expr)` | Allow when the access expression is true. |
| `settings_gate(key)` | 503 when that settings flag is truthy. |
| `env_gate(name, block_when_truthy=True)` | 503 when the environment variable's truth matches. |
| `rate_limit(max_requests, window_seconds, by_user=False)` | 429 when the window is exhausted. `by_user=True` keys on the authenticated id and falls back to the client IP. |

Calling a `Middleware` as `mw(req, res)` returns `True` when the chain should continue.

## Request

| Member | Returns |
|---|---|
| `method`, `path` | Verb and path. |
| `path_param(name)`, `has_path_param(name)`, `has_path_params()` | Route placeholders. |
| `query_param(name)`, `has_query_param(name)` | Query string. Missing values are `""`. |
| `header(name, default="")`, `has_header(name)` | Headers. `hasHeader` is the same check. |
| `cookie(name)` | One cookie, or `""`. |
| `bearer_token()` | Token after `Bearer `. |
| `auth_token()` | Bearer token, otherwise the `mb_token` cookie. |
| `body()` | Raw body text. |
| `json()` | Parsed JSON body. Malformed JSON raises `MantisError` 400. |
| `is_multipart()` | `True` for `multipart/form-data`. |
| `form_data()` | List of `{name, filename, content_type, content}`. `content` is `str` when it is UTF-8, otherwise `bytes`. |
| `file_metadata()` | File metadata dict after `parse_form`. |
| `parse_form(entity)` | Map multipart fields onto an entity. No-op for JSON. |
| `write_files(entity)` / `undo_files(entity)` | Store or roll back uploaded files for that entity name. |
| `remote_addr`, `remote_port`, `local_addr`, `local_port` | Peer and local socket. |
| `is_guest()`, `is_admin()`, `is_user()` | Auth flags set by auth middleware. |
| `has_key(key)`, `get(key)`, `get_or(key, default)`, `set(key, value)` | Per-request attributes. `set` stores a JSON value. `get` reads JSON values and plain strings, bools, and numbers left by C++ middleware (including `auth` and `verification`). |

```python
@app.router.post("/upload/{entity}")
def upload(req, res):
    entity = app.entity(req.path_param("entity"))
    req.parse_form(entity)
    req.write_files(entity.name)
    res.json(200, req.file_metadata())
```

## Response

| Member | Effect |
|---|---|
| `json(status, data)` | JSON body. `data` must be JSON-compatible. |
| `text(status, body)` / `html(status, body)` | Text or HTML. |
| `send(status, data="", content_type="text/plain")` | Body plus content type. |
| `empty(status=204)` | No body. |
| `redirect(url, status=302)` | Redirect this response. This is not `router.redirect`. |
| `set_header(name, value)`, `header(name, default="")`, `has_header(name)` | Response headers. |
| `set_content(body, content_type)` | Body and content type without a status change. |
| `set_file(path, content_type=None)` | Stream a file from disk. |
| `set_auth_cookie(token, max_age)` / `clear_auth_cookie()` | HttpOnly `mb_token` cookie. |
| `status`, `body` | Read/write status code and body. |
| `location` | `Location` header. |

## Database

`app.db.query(sql, *binds)` returns a list of dicts. Each bind is a dict of named parameters (`:name` in the SQL). Values are `None`, `bool`, `int`, `float`, or `str`. The GIL is released while the query runs.

```python
rows = app.db.query(
    "SELECT id, title FROM notes WHERE title = :title",
    {"title": "Hello"},
)
```

`connected` and `connection_string` are read-only.

`transaction()` leases one pooled session and starts a transaction. Use it as a context manager. Leaving the block without an exception commits. An exception rolls the transaction back. `query` on the transaction uses that session, so it sees uncommitted writes. `app.db.query` leases a different session and does not.

```python
with app.db.transaction() as tx:
    tx.query("UPDATE notes SET title = :title WHERE id = :id",
             {"title": "Renamed", "id": note_id})
```

`commit()` and `rollback()` end the transaction early.

## Entity

`app.entity(name)` is the table. CRUD checks the schema and talks to the database directly. It does not re-run HTTP access rules. Call these from a route when the route's own middleware has already decided who is allowed in.

| Member | Purpose |
|---|---|
| `id`, `name`, `type`, `is_system`, `has_api` | Schema identity. |
| `schema`, `fields`, `rules` | Schema JSON, field list, and rules object. |
| `field(name)`, `has_field(name)` | One field definition, or whether it exists. |
| `list_rule()`, `get_rule()`, `add_rule()`, `update_rule()`, `delete_rule()` | One rule as `{mode, expr, entity}`. |
| `auth_methods()` | `{password, otp, oauth, requireEmailVerification}` for an auth entity. |
| `auth_method_disabled_reason(method)` | Empty when `password`, `otp`, or `oauth` is enabled. |
| `view_query()` | SQL for a view entity. |
| `create(record, opts=None)` | Insert and return the stored row. |
| `list(opts=None)` | Rows ordered by `id`. |
| `list_page(opts=None)` | `{"items", "has_more"}`. |
| `read(id, opts=None)` | One row, or `None`. |
| `update(id, data, opts=None)` | Patch and return the row. |
| `remove(id)` / `delete(id)` | Delete one row. |
| `count(filter=None)` | Row count. `filter` is `{field: value}`. |
| `is_empty()`, `record_exists(id)` | Existence checks. |
| `find_by_columns(value, columns)` | First row matching `value` in any of those columns, or `None`. |

List options:

- `pagination.limit` — page size from 1 to 500, default 50
- `pagination.after` — id of the last row from the previous page
- `filter` — `{field: value}` equality filters, as a dict or a JSON string

```python
page = app.entity("notes").list_page({
    "pagination": {"limit": 20, "after": last_id},
    "filter": {"title": "Hello"},
})
```

## Auth

`app.auth` issues JWTs and owns API keys and OAuth.

| Method | Purpose |
|---|---|
| `create_token(claims, timeout=-1)` | Sign a JWT. `claims` needs `id` and `table`. `timeout` is seconds; `-1` uses the entity default. |
| `verify_token(token)` | Claims plus `verified` and `error`. |
| `delete_session(session_id)` | Invalidate a session row. |
| `refresh_session(session_id, entity, user_id)` | Rotate the session and return the new token payload. |
| `session_timeout(entity, timeout=-1)` | Lifetime in seconds. |
| `request_login_otp(entity, email)` | Email a one-time sign-in code. Unknown addresses still return `{"sent": true}`. |
| `login_with_otp(entity, email, otp)` | Exchange the code for `{"token", "user"}`. |
| `request_email_verification(entity, email, host)` | Send a verification link. `host` is used when `siteDomain` is empty. |
| `confirm_email_verification(entity, token)` | `{"verified": true}`. |
| `request_password_reset(entity, email, host)` | Send a reset link. |
| `confirm_password_reset(entity, token, new_password)` | `{"reset": true}`. |
| `reject_unverified_login(entity, user)` | Raise 403 when the entity requires a verified email and the account is not verified. |
| `notify_email_verification(entity, user_id)` | Send a verification link when SMTP is configured. Missing SMTP is ignored. |

```python
token = app.auth.create_token({"id": user["id"], "table": "users"}, timeout=3600)
claims = app.auth.verify_token(token)
```

OTP, verification, and reset calls require SMTP. See [Authentication](auth.md) for the HTTP equivalents.

### API keys

`app.auth.api_keys` stores hashed keys. `create` and `create_admin` return the raw `key` once.

| Method | Purpose |
|---|---|
| `create(entity, user_id, label, permissions=None, expires_at="")` | Entity-scoped key. |
| `list(entity, user_id)` | Metadata only. |
| `revoke(key_id, entity, user_id)` | Revoke one key owned by that user. Pass `user_id=""` to skip the owner check. |
| `revoke_by_id(key_id)` | Revoke any key. |
| `create_admin(user_id, label, permissions=None, expires_at="")` | Admin key. |
| `list_admin()` | Admin key metadata. |
| `revoke_admin(key_id)` | Revoke an admin key. |

`permissions` is a list. `expires_at` is a timestamp string, or `""` for no expiry.

### OAuth

`app.auth.oauth` configures providers and completes login. Client secrets stay in the database. The encryption key is not exposed.

| Method | Purpose |
|---|---|
| `add_provider(provider)` / `update_provider(provider_id, updates)` / `remove_provider(provider_id)` / `list_providers()` | Admin provider registry. |
| `enable_provider(entity, provider_id)` / `disable_provider(entity, provider_id)` | Attach a provider to an auth entity. |
| `providers(entity)` | Providers enabled for that entity. |
| `authorize_url(entity, provider, redirect_uri)` | Authorize URL plus stored PKCE state. |
| `handle_callback(entity, provider, code, state)` | Exchange the callback for a session. |
| `link_account(entity, user_id, provider, code, state)` | Link a provider to an existing user. |
| `unlink_account(entity, user_id, provider)` | Remove that link. |
| `linked_accounts(entity, user_id)` | Linked identities. |

## Files

`app.files` reads and writes entity upload directories under the data directory. Paths are checked for traversal.

| Method | Purpose |
|---|---|
| `base_dir` | Root directory for entity files. |
| `create_dir(entity)` / `rename_dir(old, new)` / `delete_dir(entity)` | Entity directories. |
| `dir_path(entity, create_if_missing=False)` | Absolute directory. |
| `file_path(entity, filename)` | Absolute file path. |
| `get_file_path(entity, filename)` | That path when the file exists, otherwise `None`. |
| `file_exists(entity, filename)` / `remove_file(entity, filename)` / `remove_files(entity, filenames)` | Existence and deletion. |
| `canonical_path(path)` | Canonical path inside the files root, or `MantisError` 400. |
| `is_canonical(path)` | Whether `canonical_path` would succeed. |

## Settings

`app.settings` is the same record as the admin Settings page and `PATCH /api/v1/sys/settings/config`.

| Method | Purpose |
|---|---|
| `get(key)` | One top-level value. Missing keys are `None`. A stored SMTP password is returned as `"********"`. |
| `set(key, value)` | Persist a JSON value and apply it. Pass `"********"` or omit `smtp.password` to keep the stored password. Pass `""` to clear it. |
| `configs()` | Full settings dict with the SMTP password redacted. |
| `reload()` | Re-read the database and apply SMTP and CORS again. |

```python
app.settings.set("smtp", {
    "host": "smtp.example.com",
    "port": 587,
    "user": "mailer",
    "password": "s3cret",
    "from": "app@example.com",
    "tls": True,
})
```

`MB_DISABLE_CONFIG_MUTATIONS` blocks the dashboard and the PATCH route. `settings.set` still writes, because it runs inside the process.

## Logs

`app.logs.trace`, `debug`, `info`, `warn`, `critical`, and `error` take `(origin, message, details="", data=None)`. `error` is `critical`. `data` is a JSON value stored with the log row.

```python
app.logs.info("Notes", "Created", details=note["id"], data={"title": note["title"]})
```

## Realtime

`app.rt.notify_change()` wakes the database worker so it drains the change log. `add_hooks(entity)` and `drop_hooks(entity)` install or remove the database triggers or poll hooks for one table. `broadcast(event)` and `app.router.broadcast(event)` push an event to current SSE and WebSocket subscribers without waiting for the database worker.

`app.router.sse` is the session manager:

| Member | Purpose |
|---|---|
| `broadcast(event)` | Fan out one event. |
| `session_count()` | Active SSE sessions. |
| `remove_session(client_id)` | Close one SSE session. |
| `is_running()` | Whether cleanup is running. |
| `ws_connection_count()` | Active WebSocket connections. |

Clients still connect through `GET /api/v1/realtime` and the WebSocket endpoint. See the [API Reference](api.md).

## Mailer

`app.mailer` sends SMTP mail. `configure(smtp)` accepts `host`, `port`, `user`, `password`, `from`, and `tls`. `apply_settings(configs)` reads the `smtp` object out of a full settings dict. `is_configured` reports whether a host is set. `config` is that SMTP dict with a non-empty password replaced by `"********"`.

| Method | Purpose |
|---|---|
| `send(to, subject, html="", text="")` | Deliver one message. |
| `send_test(to)` | Send the fixed "Hey, it works!" message. |
| `load_template(name, variables=None)` | Render a file from `templates_dir`. `variables` is a dict of strings. |
| `sent_messages()` | Messages captured by the mock transport (`host` `mock` or `localhost` port 0). Each item is `{to, subject, html, text}`. |
| `clear_sent_messages()` | Drop captured messages. |
| `check_rate_limit(email, max_per_hour=3)` | Record an attempt and return whether it is still allowed. |
| `clear_rate_limits()` | Reset the in-memory rate limit. |

`send` raises `MantisError` when SMTP is missing, the address is invalid, the rate limit trips, or delivery fails.

## Utils

`mantisbase.utils` mirrors the C++ helpers:

| Function | Purpose |
|---|---|
| `generate_time_based_id()` | Time-based id. |
| `generate_readable_time_id()` | Time-based id that stays readable. |
| `generate_short_id(length=16)` | Short random id. |
| `get_env(key, default="")` | Environment variable or default. |
| `sanitize_filename(name, max_len=50, id_len=12, separator="_")` | Safe unique filename. |
| `hash_password(password)` / `verify_password(password, stored_hash)` | Password hash used by auth entities. |
| `is_allowed_file_extension(extension)` | Upload allow-list check. |
| `safe_content_type(extension)` | MIME type safe to send for that extension. |
| `str_to_bool(value)` | `true`, `1`, `on`, `yes` and their opposites. |

## Threading

Route handlers run on MantisBase worker threads. The GIL is acquired around each Python handler, middleware, and callback, and released around database and filesystem work. Python handlers therefore serialize on the GIL the same way a Flask threaded server does.

Do not call `stop()` from inside a handler on the server thread that is running that handler. Call it from another thread, or from the thread that called a non-blocking `start()`.

## Building from source

The wheel links the MantisBase shared library. Build and install that library first (`cmake --install` is required; a plain build does not install it):

```bash
cmake -B build -DMB_BUILD_SHARED_LIB=ON
cmake --build build
cmake --install build --prefix ./install

pip install -e bindings/python \
  --config-settings=cmake.define.CMAKE_PREFIX_PATH=$PWD/install
```

At runtime the loader must find `libmantisbase.so`, for example `export LD_LIBRARY_PATH=$PWD/install/lib`.

The extension needs CMake >= 3.22, a C++20 compiler, and Python >= 3.9. `nanobind` and `scikit-build-core` are fetched by pip. Wheel production is described in `.github/workflows/bindings-python.yml`.
