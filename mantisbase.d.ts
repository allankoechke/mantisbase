/**
 * MantisBase JavaScript API type definitions.
 *
 * Copy this file next to your server scripts (the `scripts` directory next to
 * the `mantisbase` binary, or your custom `--scriptsDir`) to get type hinting,
 * autocompletion, and inline docs in editors that support TypeScript-style
 * checking of JavaScript (VS Code, Cursor, etc.).
 *
 * Requires MantisBase built with `MB_SCRIPTING_ENABLED=ON`.
 * Entry script: `main.mb.js` (deprecated fallback: `index.mantis.js`).
 *
 * This file is types-only: it is never executed by the server.
 * @see doc/scripting.md for the full scripting guide.
 */

/** HTTP methods accepted by `Router.addRoute`. */
type MbHttpMethod = "GET" | "POST" | "PATCH" | "PUT" | "DELETE";

/** 3xx status codes accepted by `Router.redirect` / `MbResponse.redirect`. */
type MbRedirectStatus = 300 | 301 | 302 | 303 | 304 | 307 | 308;

/** Route handler: receives the request and response objects. */
type MbHandler = (req: MbRequest, res: MbResponse) => void;

/**
 * Route middleware: same signature as a handler but returns a boolean.
 * - `true`: continue to the next middleware / handler.
 * - `false`: abort route execution (the middleware should set the response).
 */
type MbMiddleware = (req: MbRequest, res: MbResponse) => boolean;

/** Method constraint for `MbRouter.redirect`: one method or a list (`[]` allows all). */
type MbRedirectMethods = string | string[];

/** A database bind object for `db.query(sql, ...binds)` (`:name` <-> `name`). */
type MbDbBinds = Record<string, any>;

/**
 * Result of `db.query(...)`:
 * - `null` when no rows match,
 * - a single row object when exactly one row matches,
 * - an array of row objects otherwise.
 */
type MbQueryResult = any;

/** Global application object. */
interface MantisBaseApp {
    /** HTTP server host (`0.0.0.0` by default). */
    host: string;
    /** HTTP server port (`7070` by default). */
    port: number;
    /** Database connection pool size (2 for SQLite, 10 for PostgreSQL). */
    poolSize: number;
    /** Directory for serving static files (html, css, js, ...). */
    publicDir: string;
    /** Directory for SQLite files and file assets linked in records. */
    dataDir: string;
    /** Developer mode flag (logs trace information). Read-only. */
    readonly devMode: boolean;
    /** Database type in use (`sqlite3` by default). Read-only. */
    readonly dbType: string;
    /** Secret key used to sign JWT tokens. Read-only. */
    readonly secretKey: string;
    /** Running MantisBase version. Read-only. */
    readonly version: string;

    /** Close the application gracefully. */
    close(): void;
    /** Close the application gracefully with an exit code and reason. */
    quit(exitCode: number, reason: string): void;
    /** Database unit instance. */
    db(): MbDatabase;
    /** Router unit instance. */
    router(): MbRouter;
    /** Key/value app configuration unit. */
    settings(): MbSettings;
    /** JWT and session helpers unit. */
    auth(): MbAuth;
    /** File path helpers for entity uploads unit. */
    files(): MbFiles;
    /** Structured logging unit. */
    logs(): MbLogs;
    /** Realtime change notifications unit. */
    rt(): MbRealtime;
    /** Load an additional script relative to `scriptsDir`. */
    loadScript(path: string): void;
}

/** Router unit returned by `app.router()`. */
interface MbRouter {
    /**
     * Register a route for `method` on `path` (must start with `/`)
     * with `handler` and optional middlewares (run in order).
     */
    addRoute(method: MbHttpMethod, path: string, handler: MbHandler, ...middlewares: MbMiddleware[]): void;
    /** Shorthand for `addRoute("GET", ...)`. */
    get(path: string, handler: MbHandler, ...middlewares: MbMiddleware[]): void;
    /** Shorthand for `addRoute("POST", ...)`. */
    post(path: string, handler: MbHandler, ...middlewares: MbMiddleware[]): void;
    /** Shorthand for `addRoute("PUT", ...)`. */
    put(path: string, handler: MbHandler, ...middlewares: MbMiddleware[]): void;
    /** Shorthand for `addRoute("PATCH", ...)`. */
    patch(path: string, handler: MbHandler, ...middlewares: MbMiddleware[]): void;
    /** Shorthand for `addRoute("DELETE", ...)`. Bracket notation (`router["delete"](...)`) also works. */
    delete(path: string, handler: MbHandler, ...middlewares: MbMiddleware[]): void;
    /**
     * Redirect requests from `from` to `to` (relative path or absolute URL)
     * without a handler.
     * @param status 3xx status code (default `302`).
     * @param methods method name or array of method names allowed for the
     * redirect (default `["GET"]`; pass `[]` to allow all methods).
     */
    redirect(from: string, to: string, status?: MbRedirectStatus, methods?: MbRedirectMethods): void;
    /** Push a change event (JSON string) to all realtime subscribers. */
    broadcastChange(eventJson: string): void;
}

/** Incoming request object (`MbRequest` in C++). */
interface MbRequest {
    /** Raw request body. */
    readonly body: string;
    /** Request method (`GET`, `POST`, ...). */
    readonly method: string;
    /** Request path. */
    readonly path: string;
    /** Remote peer address. */
    readonly remoteAddr: string;
    /** Remote peer port. */
    readonly remotePort: number;
    /** Local server address. */
    readonly localAddr: string;
    /** Local server port. */
    readonly localPort: number;

    /** `true` when the `Authorization`-style header `key` exists. */
    hasHeader(key: string): boolean;
    /** Header value for `key`, or `def` when missing. */
    getHeader(key: string, def?: string, index?: number): string;
    /** Header value for `key` as a uint64, or `def` when missing/unparsable. */
    getHeaderU64(key: string, def?: number, index?: number): number;
    /** Number of values for header `key`. */
    getHeaderCount(key: string): number;
    /** `true` when query param `key` exists. */
    hasQueryParam(key: string): boolean;
    /** First query param value for `key`. */
    getQueryParam(key: string): string;
    /** Number of values for query param `key`. */
    getQueryParamCount(key: string): number;
    /** `true` when route path param `key` exists (e.g. `:id`). */
    hasPathParam(key: string): boolean;
    /** Route path param value for `key`. */
    getPathParam(key: string): string;
    /** Number of values for path param `key`. */
    getPathParamCount(key: string): number;
    /** `true` when the request body is `multipart/form-data`. */
    isMultipartFormData(): boolean;

    /** `true` when the per-request context store contains `key`. */
    hasKey(key: string): boolean;
    /** Store `value` in the per-request context store (int, float, double, string, object). */
    set(key: string, value: any): void;
    /** Read `value` from the per-request context store. */
    get(key: string): any;
    /** Read `key` from the context store, or `defaultValue` when missing. */
    getOr(key: string, defaultValue: any): any;
}

/** Outgoing response object (`MbResponse` in C++). */
interface MbResponse {
    /** Response body. */
    body: string;

    /** `true` when response header `key` exists. */
    hasHeader(key: string): boolean;
    /** Response header value for `key`, or `def` when missing. */
    getHeader(key: string, def?: string, index?: number): string;
    /** Response header value for `key` as a uint64. */
    getHeaderU64(key: string, def?: number, index?: number): number;
    /** Number of values for response header `key`. */
    getHeaderCount(key: string): number;
    /** Set response header `key` to `value`. */
    setHeader(key: string, value: string): void;
    /**
     * Redirect this in-flight request to `url` with `status` (default `302`).
     * For whole-path redirects without a handler, see `MbRouter.redirect`.
     */
    redirect(url: string, status?: MbRedirectStatus): void;
    /** Set the response body with an explicit content type. */
    setContent(content: string, contentType: string): void;
    /** Stream a file from disk as the response body. */
    setFileContent(path: string): void;
    /** Send a response with `status`, `data`, and `contentType`. */
    send(status: number, data?: string, contentType?: string): void;
    /** Send the standard `{ status, error, data }` JSON envelope. */
    json(status: number, data: any): void;
    /** Send an HTML body. */
    html(status?: number, data?: string): void;
    /** Send a plain-text body. */
    text(status?: number, data?: string): void;
    /** Send an empty body (default `204 No Content`). */
    empty(status?: number): void;
}

/** Database unit returned by `app.db()`. */
interface MbDatabase {
    /** `true` when connected to the database. Read-only. */
    readonly connected: boolean;
    /** Lease a database session for executing SQL directly. */
    session(): MbDbSession;
    /**
     * Execute `sql` with optional `:named` bind objects.
     * @example db.query("SELECT * FROM __settings WHERE id = :id", { id: "123" })
     */
    query(sql: string, ...binds: MbDbBinds[]): MbQueryResult;
}

/** Leased database session (`soci::session` in C++). */
interface MbDbSession {
    /** Release the leased session back to the pool. */
    close(): void;
    /** Reconnect the session. */
    reconnect(): void;
    /** `true` when the session is connected. Read-only. */
    readonly connected: boolean;
    begin(): void;
    commit(): void;
    rollback(): void;
    getQuery(): string;
    getLastQuery(): string;
    getLastQueryContext(): string;
    gotData(): boolean;
    getBackendName(): string;
    emptyBlob(): any;
}

/** Key/value app configuration unit returned by `app.settings()`. */
interface MbSettings {
    /** Read a setting as a JSON-encoded string. */
    get(key: string): string;
    /** Write a setting (value as a JSON-encoded string). */
    set(key: string, jsonValue: string): void;
    /** All configs as a JSON-encoded string (secrets redacted). */
    configs(): string;
    /** Reload configs from the database. */
    reload(): void;
}

/** JWT and session helpers unit returned by `app.auth()`. */
interface MbAuth {
    /** Create a JWT from a JSON-encoded claims string. Returns the token. */
    createToken(claimsJson: string, timeoutSeconds?: number): string;
    /** Verify a token. Returns the claims as a JSON-encoded string. */
    verifyToken(token: string): string;
    /** Revoke a session. Returns `true` on success. */
    deleteSession(sessionId: string): boolean;
    /** Refresh a session. Returns the new state as a JSON-encoded string. */
    refreshSession(oldSessionId: string, entityName: string, userId: string): string;
    /** Session timeout in seconds for `entityName` (or set it when `timeout` is given). */
    sessionTimeoutSeconds(entityName: string, timeout?: number): number;
}

/** File path helpers for entity uploads, returned by `app.files()`. */
interface MbFiles {
    /** Directory for `entityName` uploads (created when `createIfMissing`). */
    dirPath(entityName: string, createIfMissing?: boolean): string;
    /** Absolute path of `filename` under `entityName`. */
    filePath(entityName: string, filename: string): string;
    /** Resolved path of `filename` under `entityName`, or empty when missing. */
    getFilePath(entityName: string, filename: string): string;
    /** Delete `filename` under `entityName`. Returns `true` on success. */
    removeFile(entityName: string, filename: string): boolean;
}

/** Structured logging unit returned by `app.logs()`. */
interface MbLogs {
    info(message: string): void;
    warn(message: string): void;
    error(message: string): void;
    debug(message: string): void;
    trace(message: string): void;
}

/** Realtime unit returned by `app.rt()`. */
interface MbRealtime {
    /** Wake the realtime worker to drain change events immediately. */
    notifyChange(): void;
}

/** C++ middleware factories. Each returns a `(req, res) => boolean` function. */
interface MbMiddlewares {
    getAuthToken(): MbMiddleware;
    hydrateContextData(): MbMiddleware;
    resolveSchema(): MbMiddleware;
    resolveAuthEntity(): MbMiddleware;
    resolveEntity(): MbMiddleware;
    hasAccess(entity: string): MbMiddleware;
    requireEntityAuth(entity: string): MbMiddleware;
    requireAdminOrEntityAuth(entity: string): MbMiddleware;
    requireAdminAuth(): MbMiddleware;
    requireGuestOnly(): MbMiddleware;
    hasEntityAccess(): MbMiddleware;
    requireExprEval(expr: string): MbMiddleware;
    settingsFeatureGate(key: string): MbMiddleware;
    rejectViewMutations(): MbMiddleware;
}

/** Utility functions. */
interface MbUtils {
    /** Time-based unique id. */
    generateTimeBasedId(): string;
    /** Readable time-based id. */
    generateReadableTimeId(): string;
    /** Short random id of `charCount` characters (default 16). */
    generateShortId(charCount?: number): string;
    /** Environment variable `key`, or `defaultValue` when unset. */
    getEnvOrDefault(key: string, defaultValue: string): string;
    /** Strip unsafe characters from `fileName`. */
    sanitizeFilename(fileName: string): string;
    /** Hash `password` for storage. */
    hashPassword(password: string): string;
    /** `true` when `password` matches `storedHash`. */
    verifyPassword(password: string, storedHash: string): boolean;
}

/** Console output (goes to the server stdout). */
interface MbConsole {
    (...args: any[]): void;
    /** Alias of `info`. */
    log(...args: any[]): void;
    info(...args: any[]): void;
    trace(...args: any[]): void;
}

declare const app: MantisBaseApp;
declare const middlewares: MbMiddlewares;
declare const utils: MbUtils;
declare const console: MbConsole;

/**
 * Optional lifecycle hooks — define any of these in your script:
 * - `onServerStart()`: runs after scripts load, before listening.
 * - `onServerShutdown()`: runs before scripting engine is destroyed.
 * - `onRecordCreated(entity, recordId)` / `onRecordUpdated(entity, recordId)`:
 *   fired on record mutations.
 */
declare var onServerStart: (() => void) | undefined;
declare var onServerShutdown: (() => void) | undefined;
declare var onRecordCreated: ((entity: string, recordId: string) => void) | undefined;
declare var onRecordUpdated: ((entity: string, recordId: string) => void) | undefined;
