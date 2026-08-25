# mantisbase (Node.js)

Node.js bindings for [mantisbase](https://github.com/allankoechke/mantisbase), a
C++20 single-binary backend-as-a-service. Require it like Express and drive the
whole server from JavaScript.

```bash
npm install mantisbase
```

```js
const { App } = require('mantisbase')

const app = new App({ port: 8080, dataDir: './data' })

app.router.get('/hello/:name', (req, res) => {
    res.json(200, { message: `Hello, ${req.pathParam('name')}!` })
})

app.router.post('/items', async (req, res) => {
    const body = req.json()
    const rows = await app.db.query(
        'INSERT INTO items (name) VALUES (:name) RETURNING *',
        { name: body.name }
    )
    res.json(201, rows[0])
})

app.start()  // non-blocking; Drogon runs on a background thread
```

TypeScript declarations ship with the package (`index.d.ts`).

## API

### `new App(config)`

Config keys mirror the CLI flags: `port`, `host`, `dataDir`, `dbType`, `dbUrl`,
`publicDir`, `scriptsDir`, `migrationsDir`, `poolSize`, `dev`,
`skipAdminSetup`. `secretKey` sets the `MB_JWT_SECRET` environment variable.

- `start()` — start the HTTP server on a background thread (non-blocking).
- `stop()` — shut the server down and join the thread.
- `router` / `db` — the `Router` and `Database` instances.

### `Router`

`get(path, handler)`, `post(...)`, `patch(...)`, `delete(...)`. Handlers may be
`async`; the response is sent once the returned Promise settles.

### `MantisRequest`

`pathParam(name)`, `queryParam(name)`, `header(name)`, `json()`, `body()`, and
the read-only properties `method`, `path`, `remoteAddr`.

### `MantisResponse`

`json(status, data)`, `html(status, body)`, `text(status, body)`,
`send(status, body, contentType)`, `redirect(url, status)`,
`setHeader(name, value)`.

### `Database`

`query(sql, ...params)` where each param is an object of named binds. Returns a
Promise resolving to an array of row objects keyed by column name. The query
runs on a libuv worker thread, so the Node.js event loop is never blocked.

## Threading

Route handlers are dispatched from mantisbase's C++ worker threads onto the
Node.js event loop via `napi_threadsafe_function`. The originating C++ thread
blocks until the handler (and its Promise, if async) completes, so concurrent
request capacity is bounded by the server's thread pool size.

## Building from source

```bash
cd bindings/node
npm install       # prebuild-install, falling back to a cmake-js build
npx cmake-js build
```

Requires CMake >= 3.22, a C++20 compiler, and the mantisbase shared library
(`cmake -DMB_BUILD_SHARED_LIB=ON -DMB_BUILD_BINDINGS=ON ..`).

## License

MIT
