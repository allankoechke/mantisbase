@page docs_scripting Scripting

On server startup, MantisBase loads JavaScript from your configured scripts directory. The primary entry file is **`main.mb.js`**. If that file is missing, MantisBase falls back to **`index.mantis.js`** and logs a deprecation warning.

Scripts run once at startup. Custom routes registered via `app.router().addRoute()` are stored in the Duktape heap and invoked by the C++ HTTP layer when matching requests arrive.

The scripts directory is resolved as follows:
- By default: a `scripts` folder next to the `mantisbase` binary
- Override with `--scriptsDir /some/path` (see [Command-line options](@ref cmd))

### Build and runtime switches

| Switch | Effect |
|---|---|
| `MB_SCRIPTING_ENABLED=ON` (CMake) | Compile Duktape bindings into the binary |
| `--disable-scripting` / `--no-scripting` | Skip VM init and script load at runtime |
| `MB_SCRIPTING_DISABLED=1` | Same as `--disable-scripting` |

### Thread safety

All JavaScript execution uses a **single Duktape context** guarded by a mutex. Custom route handlers and middleware run under this lock on Drogon worker threads. Keep handlers fast; delegate heavy or blocking work to C++ APIs or async patterns outside the script lock.

## Application
Mantis exposes a global object `app` that provides access to the following properties. Despite them being both `READ` and `WRITE`, note that some of these properties are evaluated only once when initializing, as such, update to the values will not have any impact.
- `app.host`:Get HTTP server host, by default `0.0.0.0`.
- `app.port`: Get HTTP server port, by default `7070`.
- `app.poolSize`: Get the database connection pool size, 2 for SQLite, 10 for PostgreSQL.
- `app.publicDir`: Get/Set the public directory for serving static files (html, css, js, etc.)
- `app.dataDir`: Get/Set the directory for storing database files in _SQLite_ backend and file assets linked in database records.
- `app.devMode`: Get/Set the developer mode to log trace information on the console.
- `app.dbType`: Get the database type used, by default it's _SQLite_.
- `app.secretKey`: Get/Set secret key used in signing JWT tokens.
- `app.version`: Get Mantis version.

Additionally, some methods are exposed on the `app` object:
- `app.close()`: Close the application gracefully.
- `app.quit(exitCode, reason)`: Close the application gracefully with a _exitCode_ and a _reason_.
- `app.db()`: Get the database unit instance.
- `app.router()`: Get the router unit instance.
- `app.settings()`: Key/value app configuration (`get`, `set`, `configs`, `reload`).
- `app.auth()`: JWT and session helpers (`createToken`, `verifyToken`, `deleteSession`, `refreshSession`, `sessionTimeoutSeconds`).
- `app.files()`: File path helpers for entity uploads (`dirPath`, `filePath`, `getFilePath`, `removeFile`).
- `app.logs()`: Structured logging (`info`, `warn`, `error`, `debug`, `trace`).
- `app.rt()`: Realtime change notifications (`notifyChange`).
- `app.loadScript(path)`: Load an additional script relative to `scriptsDir`.

## Database
The `DatabaseUnit` object returned from the `app.db()` method exposes the following methods and properties:
```js
const db = app.db() // Get db unit instance
```
- `db.connected`: Property to read the connection state to the database.
- `db.session()`: Request a database session object which is used for executing SQL queries and other database operations. Returns an instance of `soci::session` object.
- `db.query(sql_query, [bind objects])`: Useful for executing SQL queries on database directly. Expects the first argument to a valid SQL query followed by optional values to bind if any.

```js
const db = app.db();
const items = db.query("SELECT * FROM __settings");
console.log("Settings: ", JSON.stringify(items));

// Find a specific data or value based on some column
// The bind value name in SQL (`:id` here) should match the bind's object key `id`
const r = db.query("SELECT * FROM __settings WHERE id = :id", { id: "12509202836555480207" });
// db.query("SELECT * FROM __settings WHERE id = :id && updated = :updated", { id: "12509202836555480207" }, { updated: "2025-10-24 01:32:49" });
console.log("Settings:  ", r.id); // `r` may be null if not found.
```

### Session
The session method returns an instance that exposes the following methods and properties:
```js
const sess = db.session() // Lease a session for executing db actions
```
- `sess.close()`: Close a specific database session. Note that, if the poolSize is greater than 1, this only closes the _sess_ we borrowed earlier.
- `sess.reconnect()`: Reconnect the session instance.
- `sess.connected`: Property to get session instance connection status.
- `sess.begin`
- `sess.commit`
- `sess.rollback`
- `sess.getQuery`
- `sess.getLastQuery`
- `sess.getLastQueryContext`
- `sess.gotData`
- `sess.getLastInsertId`
- `sess.getBackendName`
- `sess.emptyBlob`

## Router
The router instance allows binding handlers and optional middlewares to given routes from JavaScript.
- `app.router().addRoute(method, path, handler, [middlewares])`: Create a route for a given HTTP method, on a given *path* with the given handler function with optional middleware functions.

```js
app.router().addRoute("GET", "/test", function (req, res){
    // ...
}) 

// With one or more middlewares
app.router().addRoute("GET", "/test", function (req, res){
    // ...
}, function (req, res){
    // ...
    return true
}) 
```

## Requests
To add a new request endpoint in JS, Mantis exposes a `addRoute` method having the following signature:

```js
app.router().addRoute(method, path, function_handler) 
// With Middlewares
app.router().addRoute(method, path, function_handler, middleware1, middleware2, ...)
```
Let's add a `/test` route.
```js
app.router().addRoute("GET", "/test", function (req, res){
    // Request Handler Method
    res.json(200, JSON.stringify({a: 5, b: 4}))
}) 

// With one or more middlewares
app.router().addRoute("GET", "/test", function (req, res){
    // Request Handler Method
    res.json(200, JSON.stringify({a: 5, b: 4}))
}, function (req, res){
    // Handler Body
    
    // If the middleware is successful, return true
    // if we encounter an error, return false to abort route execution
    return true
}) 
```

To access the request metadata as well as set response metadata, the handler or middleware method passes in two arguments, the request object (shown as `req` above) and the response object (shown as `res` above).

The `req` object maps to `MantisRequest` and `res` maps to `MantisResponse` class in C++.

### Middlewares
Middlewares in Mantis carries the same function signature as the request handler with just one tiny difference. The middlewares return a `bool` type, indicating whether or not the middleware executed successfully. That is;
- Return `true`: Middleware execution succeeded, proceed to the next handler and or handler.
- Return `false`: Middleware execution failed, abort request.

```js
function checkIfAuthenticated(req, res) {
    var authenticated = false;

    // Do your magic here
    var token = req.getHeader("Authorization", "", 0)
    // Validate token ...
    // Other magic?
    // authenticated = ....
    
    return authenticated
}

// Middleware is executed first before the handler
app.router().addRoute("POST", "/foo/bar", function (req, res) {}, checkIfAuthenticated)
```

> Warning
> Note that, middlewares are executed from the first to the last in that order. That is,
> `app.router().addRoute(...., middleware_1, middleware_2, function(req, res){ return true}, ...)` executing from `middleware_1` to `middleware_n` the finally invoking the handler.

### MantisRequest Methods/Properties
- `req.hasHeader("Authorization")`: return true/false
- `req.getHeader("Authorization", "", 0)`: Return Authorization value or default
- `req.getHeaderCount("key")`: Count for header values given the header key 
- `req.setHeader("Cow", "Cow Value")`
- `req.hasQueryParam("key")`: return true/false
- `req.getQueryParam("key")`: Return header value given the key
- `req.getQueryParamCount("key")`: Return parameter value count
- `req.hasPathParam("key")`: return true/false
- `req.getPathParam("key")`: Return header value given the key
- `req.getPathParamCount("key")`: Return parameter value count
- `req.isMultipartFormData()`: Return true if request type is of Multipart/FormData
- `req.body`: Get request body data
- `req.method`: Get request method ('GET', 'POST', ...)
- `req.path`: Get request path value
- `req.remoteAddr`: Remote request address
- `req.remotePort`: Remote request port
- `req.localAddr`: Local request address
- `req.localPort`: Local request port
- `req.hasKey("key")`: Returns true if the context store contains the given key.
- `req.set("key", value)`: Store in the context store the given key and value. Note only `int`, `float`, `double`, `strings` and `objects` are supported for now.
- `req.get("key")`: Get a value from the context store given the `key`.
- `req.getOr("key", "default value")`: Returns a value if `key` is in the context store else the default value.

### MantisResponse Methods/Properties
- `res.hasHeader("Authorization")`: Check if a header `key` exists in the request header. Returns true or false.
- `res.getHeader(<key>, <default>, <index>)`: Get header value for a given `key` in the header. All three parameters are required, `key` being the header of interest, `default` being the base value if header is not found and `index` being the 0-based index for the item to return if the header value has more than one value. i.e 
```js
    const token = res.getHeader("Authorization", "", 0)
    const type = res.getHeader("content-type", "text/plain", 0)
```
- `res.getHeaderCount("key")` -> Count for header values given the header key
- `res.setHeader("Cow", "Cow Value")`
- `res.redirect("http://some-url.com", 302)`
- `res.setContent(content, content_type)`: i.e. `res.setContent("<html>...</html>", "text/html")`
- `res.setFileContent("/foo/file_path")`
- `res.send(200, "some data here", "text/plain")`
- `res.json(200, "{\"a\": 5}")`
- `res.html(200, "<html> ... </html>")`
- `res.text(200, "some text response")`
- `res.empty(204)`
- `res.body = "Some Data"`
- `res.body` -> returns `Some Data`
- `res.status` (get or set status data)
- `res.version` (get or set version value)
- `res.location` (get or set redirect location value)
- `res.reason` (get or set reason value)

## Middlewares (C++ factories)

The global `middlewares` object exposes C++ middleware factories. Each factory returns a JS function with signature `(req, res) => bool` that runs under the script mutex:

- `middlewares.getAuthToken()`
- `middlewares.hydrateContextData()`
- `middlewares.resolveSchema()`, `resolveAuthEntity()`, `resolveEntity()`
- `middlewares.hasAccess(entity)`, `requireEntityAuth(entity)`, `requireAdminOrEntityAuth(entity)`
- `middlewares.requireAdminAuth()`, `requireGuestOnly()`, `hasEntityAccess()`
- `middlewares.requireExprEval(expr)`, `settingsFeatureGate(key)`
- `middlewares.rejectViewMutations()`

```js
app.router().addRoute("GET", "/api/v1/custom/protected", handler,
    middlewares.getAuthToken(),
    middlewares.hydrateContextData()
);
```

## Utils (utility functions)
Mantis exposes an object `utils` which has the following utility functions.
- `utils.generateTimeBasedId()`
- `utils.generateReadableTimeId()`
- `utils.generateShortId(char_count)`
- `utils.getEnvOrDefault(key, default_value)`
- `utils.sanitizeFilename(file_name)`
- `utils.hashPassword(password)`
- `utils.verifyPassword(password, stored_password)`