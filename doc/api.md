@page docs_rest_api REST API Reference Guide

MantisBase provides auto-generated RESTful APIs for interacting with database entities. This document covers the entity endpoints, schema management, realtime (SSE) API for live database change notifications (SQLite and PostgreSQL), and request handling.

---

## Base URL

When MantisBase is running locally:

```
http://localhost:7070/api/v1/
```

You can configure the port and host using command-line arguments:

```bash
mantisbase serve --port 8000 --host 127.0.0.1
```

### Response Security Headers

All HTTP responses include:

- `X-Content-Type-Options: nosniff`
- `X-Frame-Options: SAMEORIGIN`
- `Referrer-Policy: strict-origin-when-cross-origin`

### Reverse Proxies and Client IP

When running behind a reverse proxy or load balancer, set `MB_TRUSTED_PROXIES` to a comma-separated list of proxy IP addresses. Only then is `X-Forwarded-For` trusted for client-IP resolution (rate limits, logging). If unset, the direct peer address is used so the header cannot be spoofed. See [Docker Guide](docker.md#environment-variables).

---

## API Namespaces

All REST endpoints live under `/api/v1/` and are grouped by namespace:

| Prefix | Description |
|--------|-------------|
| `/api/v1/auth/` | Entity user authentication: token verify (`/verify`), plus per-entity login, refresh, logout, API keys, and OAuth |
| `/api/v1/entities/` | Entity record CRUD |
| `/api/v1/schemas/` | Schema management (admin only) |
| `/api/v1/files/` | Uploaded file serving |
| `/api/v1/health` | Server health check |
| `/api/v1/sys/logs/` | System logs (admin only) |
| `/api/v1/sys/admins/` | Admin accounts, admin auth, and initial setup |
| `/api/v1/sys/api-keys/` | Admin API key management |
| `/api/v1/sys/oauth/` | OAuth provider registry and entity enablement (admin) |
| `/api/v1/sys/settings/` | Application settings |
| `/api/v1/realtime` | Server-Sent Events for live database changes |
| `WS /api/v1/realtime/ws` | WebSocket for live database changes |

---

## Entity Endpoints

MantisBase automatically exposes CRUD endpoints for each entity (table or view):

| Method | Endpoint                              | Description           |
|--------|---------------------------------------|-----------------------|
| GET    | `/api/v1/entities/<entity>`           | List all records      |
| GET    | `/api/v1/entities/<entity>/:id`       | Get a specific record |
| POST   | `/api/v1/entities/<entity>`           | Create a new record   |
| PATCH  | `/api/v1/entities/<entity>/:id`       | Update partial fields |
| DELETE | `/api/v1/entities/<entity>/:id`       | Delete a record       |

Auth-type entity responses never include the `password` field in list, get, or realtime change payloads.

### Example Requests

```bash
# List all users
curl http://localhost:7070/api/v1/entities/users

# Get specific user
curl http://localhost:7070/api/v1/entities/users/123

# Create a new user
curl -X POST http://localhost:7070/api/v1/entities/users \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "John Doe", "email": "john@example.com"}'

# Update user
curl -X PATCH http://localhost:7070/api/v1/entities/users/123 \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "Jane Doe"}'

# Delete user
curl -X DELETE http://localhost:7070/api/v1/entities/users/123 \
  -H "Authorization: Bearer <token>"
```

---

## Authentication

All entity endpoints require authentication via JWT tokens. Include the token in the `Authorization` header:

```
Authorization: Bearer <token>
```

For authentication endpoints, see [Authentication API](auth.md).

### HTTP status codes

| Code | Meaning |
|------|---------|
| `401 Unauthorized` | Missing, invalid, or expired credentials (no valid JWT or API key) |
| `403 Forbidden` | Valid credentials, but the authenticated user is not permitted to access the resource (access rules, admin-only routes, custom expressions) |
| `503 Service Unavailable` | The route is temporarily disabled (for example, by an environment gate such as `MB_DISABLE_ADMIN_MUTATIONS`) |

---

## Middlewares

Middlewares are functions that run before your route handler, allowing you to add authentication, authorization, and request processing logic.

### Default Middlewares

Every endpoint automatically has two middlewares applied globally:

1. **`getAuthToken()`** - Extracts JWT token from `Authorization` header and stores it in request context
2. **`hydrateContextData()`** - Validates token, fetches user data from database, and populates request context with user information

Additionally, entity endpoints automatically have:
3. **`hasAccess(entity_name)`** - Evaluates entity access rules to determine if the authenticated user can perform the requested operation. Called automatically by entity endpoints to confirm access rules before data query.

### Available Middlewares

You can use these middlewares when creating custom endpoints:

| Middleware | Description | Usage |
|------------|-------------|-------|
| `getAuthToken()` | Extract token from Authorization header | Applied globally to all routes |
| `hydrateContextData()` | Validate token and load user data | Applied globally to all routes |
| `hasAccess(entity_name)` | Check entity access rules | Applied automatically to entity endpoints |
| `requireAdminAuth()` | Require admin authentication | Blocks non-admin users |
| `requireExprEval(expr)` | Custom expression gate on a route | Evaluates `auth` / `req` context; **403** `"Access denied!"` when the expression is false (no upfront auth requirement) |
| `requireEntityAuth(entity_name)` | Require auth from a specific entity | **401** without valid credentials; **403** when authenticated as another entity |
| `requireAdminOrEntityAuth(entity_name)` | Require admin or entity auth | **401** without valid credentials; **403** when authenticated as neither admin nor the given entity |
| `requireGuestOnly()` | Require no authentication | Blocks authenticated users, only allows guests |
| `rateLimit(max_requests, window_seconds, use_user_id)` | Rate limiting middleware | Limits requests per time window by IP or user ID |
| `envGateMiddleware(env_var, block_when_truthy)` | Environment gate | Blocks with **503** when the env var is truthy (`true`, `1`, `on`, `yes`); otherwise the request continues |

#### `envGateMiddleware`

Blocks route execution when an environment variable matches a configured truthy value. The env var is read on each request (not cached at startup).

```cpp
// Block admin account mutations when MB_DISABLE_ADMIN_MUTATIONS is set to true/1/on/yes
router.Post("/api/v1/sys/admins", handler, {
    requireAdminAuth(),
    envGateMiddleware("MB_DISABLE_ADMIN_MUTATIONS", true)
});
```

| Env value | Behavior |
|-----------|----------|
| Unset | Normal operation |
| `false`, `0`, or any other non-truthy string | Normal operation (ignored) |
| `true`, `1`, `on`, `yes`, `t` | **503** — `{"status":503,"error":"Resource action temporarily disabled","data":{}}` |

See [Docker environment variables](docker.md#environment-variables) for production flags.

### Using Middlewares

When creating custom endpoints, you can specify middlewares as the third parameter:

```cpp
// Require admin authentication
router.Get("/api/v1/admin/stats", [](MantisRequest& req, MantisResponse& res) {
    res.sendJSON(200, {{"stats", "data"}});
}, {requireAdminAuth()});

// Require authentication from specific entity
router.Get("/api/v1/users/profile", [](MantisRequest& req, MantisResponse& res) {
    auto auth = req.getOr<json>("auth", json::object());
    std::string userId = auth["id"];
    // ... return user profile
}, {requireEntityAuth("users")});

// Allow admins OR users from specific entity
router.Get("/api/v1/posts/draft", [](MantisRequest& req, MantisResponse& res) {
    // ... return draft posts
}, {requireAdminOrEntityAuth("users")});

// Guest-only endpoint (no authentication required)
router.Get("/api/v1/public/info", [](MantisRequest& req, MantisResponse& res) {
    res.sendJSON(200, {{"info", "public data"}});
}, {requireGuestOnly()});

// Custom expression evaluation
router.Get("/api/v1/restricted", [](MantisRequest& req, MantisResponse& res) {
    // ... handler
}, {requireExprEval("auth.id != \"\" && auth.user.verified == true")});

// Rate limiting by IP address (100 requests per minute)
router.Get("/api/v1/data", [](MantisRequest& req, MantisResponse& res) {
    res.sendJSON(200, {{"data", "response"}});
}, {rateLimit(100, 60, false)});

// Rate limiting by user ID (10 requests per second)
router.Post("/api/v1/upload", [](MantisRequest& req, MantisResponse& res) {
    // ... handler
}, {rateLimit(10, 1, true)});

// Multiple middlewares (executed in order)
router.Post("/api/v1/sensitive", [](MantisRequest& req, MantisResponse& res) {
    // ... handler
}, {
    requireAdminAuth(),
    requireExprEval("req.body.priority <= 5")
});
```

### Accessing User Data in Handlers

After middlewares run, you can access authenticated user data from the request context:

```cpp
router.Get("/api/v1/me", [](MantisRequest& req, MantisResponse& res) {
    // Get auth data from context (set by middlewares)
    auto auth = req.getOr<json>("auth", json::object());
    
    if (auth["type"] == "guest") {
        res.sendJSON(401, {{"error", "Not authenticated"}});
        return;
    }
    
    // Access user information
    std::string userId = auth["id"];
    std::string userEntity = auth["entity"];
    json userData = auth["user"]; // Full user record from database
    
    res.sendJSON(200, {{"user", userData}});
});
```

> **Note**: Middlewares execute in the order they are specified. If a middleware returns `HandlerResponse::Handled`, subsequent middlewares and the handler are skipped.

---

## Schema Management API

Schema management endpoints allow you to create, read, update, and delete entity schemas. **These endpoints require admin authentication only.**

| Method | Endpoint                              | Description              |
|--------|---------------------------------------|--------------------------|
| GET    | `/api/v1/schemas`                     | List all schemas         |
| GET    | `/api/v1/schemas/:schema_name_or_id`  | Get a specific schema     |
| POST   | `/api/v1/schemas`                     | Create a new schema      |
| PATCH  | `/api/v1/schemas/:schema_name_or_id` | Update a schema           |
| DELETE | `/api/v1/schemas/:schema_name_or_id` | Delete a schema           |

### Example: Create a Schema

**Base Entity (Standard Table):**

```bash
curl -X POST http://localhost:7070/api/v1/schemas \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "posts",
    "type": "base",
    "fields": [
      {"name": "title", "type": "string", "required": true},
      {"name": "content", "type": "string"},
      {"name": "author_id", "type": "string", "required": true}
    ],
    "rules": {
      "list": {"mode": "public", "expr": "auth.id != \"\""},
      "get": {"mode": "auth", "entity": "users"},
      "add": {"mode": "auth"},
      "update": {"mode": "auth", "entity": "users,students"},
      "delete": {"mode": "custom", "expr": "auth.entity == \"mb_admins\""}
    }
  }'
```

**View Entity (SQL View):**

```bash
curl -X POST http://localhost:7070/api/v1/schemas \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "published_posts",
    "type": "view",
    "view_query": "SELECT * FROM posts WHERE status = '\''published'\''",
    "rules": {
      "list": {"mode": "public", "expr": ""},
      "get": {"mode": "public", "expr": ""}
    }
  }'
```

**Auth Entity (Authentication Table):**

```bash
curl -X POST http://localhost:7070/api/v1/schemas \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "users",
    "type": "auth",
    "fields": [
      {"name": "email", "type": "string", "required": true, "is_unique": true},
      {"name": "full_name", "type": "string"}
    ],
    "rules": {
      "list": {"mode": "auth", "expr": ""},
      "get": {"mode": "auth", "expr": ""},
      "add": {"mode": "public", "expr": ""},
      "update": {"mode": "custom", "expr": "auth.id == req.body.id"},
      "delete": {"mode": "", "expr": ""}
    }
  }'
```

### Entity Types

MantisBase supports three entity types:

| Type | Description | Fields | Special Properties |
|------|-------------|--------|-------------------|
| `base` | Standard database table | Yes | Standard CRUD operations |
| `auth` | Authentication entity | Yes | Includes password, email, and user management fields automatically |
| `view` | SQL view (read-only) | No | Requires `view_query` instead of fields |

### Entity Name Validation

Entity names must follow these rules:
- **Alphanumeric and underscores only** - Only letters, numbers, and `_` characters allowed
- **Maximum 64 characters** - Names cannot exceed 64 characters
- **Not empty** - Names must contain at least one character

Invalid names will be rejected with a 400 error.

### Field Name Validation

Field names follow the same rules as entity names:

- **Alphanumeric and underscores only** — letters, numbers, and `_`
- **Maximum 64 characters**
- **Not empty**

Invalid field names are rejected with a **400** error when creating or updating schemas.

### View Query Validation

`view` entities require a `view_query` that:

- Starts with `SELECT` (case-insensitive after trimming)
- Contains **no semicolons**
- Contains **no DDL/DML keywords** (e.g. `INSERT`, `UPDATE`, `DELETE`, `DROP`)

Invalid view queries are rejected with a **400** error.

### Updating Schemas

When updating a schema, you can add, update, or remove fields:

```bash
curl -X PATCH http://localhost:7070/api/v1/schemas/posts \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{
    "fields": [
      {"name": "new_field", "type": "string"},           // Add new field
      {"id": "field_id_123", "type": "text"},            // Update existing field by ID
      {"id": "old_field_id", "op": "delete"}              // Remove field
    ]
  }'
```

**Field Operations:**
- **Add**: Include a field with a `name` that doesn't exist
- **Update**: Include a field with an existing `id`
- **Delete**: Include a field with an existing `id` and `"op": "delete"` or `"op": "remove"`

> ⚠️ **Admin Only**: All schema endpoints require admin authentication. Regular users cannot access these endpoints.

---

## Query Parameters

Entity list endpoints use cursor-based pagination ordered by record `id` (ascending):

| Parameter | Default | Description |
|-----------|---------|-------------|
| `limit` | `50` | Number of records to return (1–500) |
| `after` | — | Cursor (`id` of the last item from the previous page) |
| `filter` | — | URL-encoded JSON object of field equality filters, e.g. `{"status":"active"}` |

**Example:**

```bash
# First page
curl "http://localhost:7070/api/v1/entities/posts?limit=20"

# Next page (using cursor from previous response)
curl "http://localhost:7070/api/v1/entities/posts?limit=20&after=<cursor>"

# Filtered list
curl "http://localhost:7070/api/v1/entities/posts?filter=%7B%22status%22%3A%22active%22%7D"
```

**Response:**

```json
{
  "status": 200,
  "data": {
    "items_count": 20,
    "limit": 20,
    "has_more": true,
    "cursor": "019c1b81-364b-7000-8120-b5416b2c42c2",
    "items": [...]
  },
  "error": ""
}
```

When `has_more` is `false`, you have reached the last page.

---

## Custom Endpoints

You can create custom API endpoints using the router:

```cpp
router.Get("/api/v1/custom", [](MantisRequest& req, MantisResponse& res) {
    res.sendJSON(200, {{"message", "Custom endpoint"}});
}, {requireAdminAuth()});
```

Check the [Embedding Guide](embedding.md) for more details.

---

## File Handling

Files uploaded via multipart/form-data are stored and can be accessed at:

```
GET /api/v1/files/<entity>/<filename>
```

See [File Handling](files.md) for more details.

---

### Foreign Key Relationships

Foreign keys allow you to establish relationships between entities. When creating or updating schemas with foreign key fields, MantisBase automatically validates the relationships.

#### Foreign Key Structure

Foreign keys are defined using a `foreign_key` object in the field definition:

```json
{
  "name": "post_id",
  "type": "string",
  "foreign_key": {
    "entity": "posts",
    "field": "id",
    "on_update": "CASCADE",
    "on_delete": "CASCADE"
  }
}
```

#### Foreign Key Properties

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `entity` | string | Yes | - | Name of the referenced entity (table) |
| `field` | string | No | `"id"` | Column name in the referenced entity |
| `on_update` | string | No | `"RESTRICT"` | Action when referenced record is updated |
| `on_delete` | string | No | `"RESTRICT"` | Action when referenced record is deleted |

#### Foreign Key Policies

Both `on_update` and `on_delete` support the following policies:

| Policy | Description |
|--------|-------------|
| `CASCADE` | Automatically update/delete related records |
| `SET NULL` | Set foreign key field to NULL when referenced record is updated/deleted |
| `RESTRICT` | Prevent update/delete if related records exist (default) |
| `NO ACTION` | Similar to RESTRICT, but checked after the operation |
| `SET DEFAULT` | Set foreign key field to its default value |

#### Foreign Key Validation

When creating or updating schemas with foreign keys, MantisBase automatically validates:

1. **Referenced Entity Exists** - The entity referenced by `foreign_key.entity` must exist
2. **Referenced Field Exists** - The field specified in `foreign_key.field` must exist in the referenced entity
3. **Type Compatibility** - Field types should be compatible (warnings issued for mismatches)

**Note:** If the referenced entity doesn't exist yet, a warning is issued but the schema is still created. The database will enforce the constraint when the DDL is executed.

#### Examples

**Example 1: Comments with Post Reference**

```json
{
  "name": "comments",
  "type": "base",
  "fields": [
    {"name": "content", "type": "string", "required": true},
    {
      "name": "post_id",
      "type": "string",
      "required": true,
      "foreign_key": {
        "entity": "posts",
        "field": "id",
        "on_delete": "CASCADE"
      }
    }
  ]
}
```

**Example 2: User Profile with User Reference**

```json
{
  "name": "profiles",
  "type": "base",
  "fields": [
    {"name": "bio", "type": "string"},
    {
      "name": "user_id",
      "type": "string",
      "required": true,
      "foreign_key": {
        "entity": "users",
        "field": "id",
        "on_update": "CASCADE",
        "on_delete": "CASCADE"
      }
    }
  ]
}
```

**Example 3: Removing a Foreign Key**

To remove a foreign key constraint, set `foreign_key` to `null`:

```json
PATCH /api/v1/schemas/comments
{
  "fields": [
    {
      "id": "post_id_field_id",
      "foreign_key": null
    }
  ]
}
```

#### Constraint Naming

Foreign key constraints are automatically named using the pattern: `fk_<table_name>_<field_name>`

For example, a foreign key on `post_id` in the `comments` table would create a constraint named `fk_comments_post_id`.

---

## System Endpoints

Most system endpoints are grouped under `/api/v1/sys/`. The health check is at `/api/v1/health`.

### Health Check

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/health` | Server health and uptime |

See [Healthcheck](healthcheck.md) for details.

### Settings

Requires **admin authentication** for all settings endpoints.

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/sys/settings/config` | Get application settings |
| PATCH | `/api/v1/sys/settings/config` | Update application settings |

PATCH returns **503** when `MB_DISABLE_CONFIG_MUTATIONS` is set to a truthy value. A successful PATCH reloads the CORS allowlist immediately (no server restart required).

The settings object contains the following fields:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `orgName` | string | `"ACME Corp"` | Organization display name |
| `siteDomain` | string | `"https://acme.example.com"` | Public site URL (OAuth callbacks, JWT audience) |
| `corsAllowedOrigins` | array of strings | `["http://localhost:3000", "http://127.0.0.1:3000"]` | Browser origins allowed for cross-origin requests with credentials |
| `maxFileSize` | integer | `10485760` | Max file upload size in bytes (10 MiB). Uploads above this are rejected with **413** |
| `logRetentionDays` | integer | `5` | Delete application logs older than this many days (hourly cleanup job) |
| `disableAdminRegistration` | boolean | `false` | When **true**, blocks creating new admin accounts via the API (`POST /api/v1/sys/admins`, `POST /api/v1/sys/admins/setup`) with **503**. When false or unset, admin API registration is allowed. Does not affect `mantisbase admins --add`. |
| `disableSchemaMutations` | boolean | `false` | When **true**, blocks schema mutations via the API (`POST`, `PATCH`, `DELETE` on `/api/v1/schemas/*`) with **503**. `GET` remains available. Does not affect `mantisbase schema` CLI commands. |
| `emailVerificationRequired` | boolean | `false` | Require email verification on registration |
| `sessionTimeout` | integer | `86400` | User session timeout in seconds (24 h) |
| `adminSessionTimeout` | integer | `3600` | Admin session timeout in seconds (1 h) |
| `jwtEnableSetIssuer` | boolean | `false` | Set JWT issuer from `orgName` |
| `jwtEnableSetAudience` | boolean | `false` | Set JWT audience from `siteDomain` |
| `smtp` | object | see below | Outbound mail configuration |

SMTP object fields: `host`, `port` (default 587), `user`, `password`, `from`, `tls`. On GET, a non-empty password is returned as `"********"`. On PATCH, send `"********"` for `smtp.password` to keep the existing value.

The GET response also includes `mantisVersion` (the running server version).

#### Cross-Origin Resource Sharing (CORS)

MantisBase enables credentialed cross-origin browser access when the request `Origin` header matches an entry in the allowlist. Allowed origins are loaded from:

1. **`corsAllowedOrigins`** in application settings (GET/PATCH `/api/v1/sys/settings/config`)
2. **`MB_CORS_ORIGINS`** environment variable — comma-separated list, merged with settings at server startup and after each settings PATCH

Example environment variable:

```bash
export MB_CORS_ORIGINS="http://localhost:3000,https://app.example.com"
```

Example settings PATCH (admin auth required):

```json
{
  "corsAllowedOrigins": [
    "http://localhost:3000",
    "https://app.example.com"
  ]
}
```

For credentialed requests (`fetch(..., { credentials: "include" })`), responses echo the exact matching origin and set `Access-Control-Allow-Credentials: true`. Wildcard `Access-Control-Allow-Origin: *` is not used.

`http://localhost:3000` and `http://127.0.0.1:3000` are different origins; list each host your frontend uses. Preflight `OPTIONS` requests are handled automatically for allowed origins.

See also [Docker configuration](docker.md) for `MB_CORS_ORIGINS` in container deployments.

### Admin Accounts

Admin account CRUD and authentication live under `/api/v1/sys/admins/`.

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/sys/admins/login` | Admin login |
| POST | `/api/v1/sys/admins/refresh` | Refresh admin token |
| POST | `/api/v1/sys/admins/logout` | Admin logout |
| POST | `/api/v1/sys/admins/setup` | Create initial admin (first boot only) |
| GET | `/api/v1/sys/admins` | List admin accounts |
| GET | `/api/v1/sys/admins/:id` | Get admin account |
| POST | `/api/v1/sys/admins` | Create admin account |
| PATCH | `/api/v1/sys/admins/:id` | Update admin account |
| DELETE | `/api/v1/sys/admins/:id` | Delete admin account |

### Logs Endpoint

The logs endpoint provides access to system logs with filtering and cursor pagination. **Requires admin authentication.** Results are ordered by log `id` (ascending).

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/sys/logs` | Get system logs with filtering and pagination |

#### Query Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `limit` | integer | `50` | Number of records per page (max 1000) |
| `after` | string | — | Cursor (`id` of last item from previous page) |
| `level` | string | — | Filter by exact log level: `trace`, `debug`, `info`, `warn`, `critical` |
| `min_level` | string | — | Filter by minimum log level (includes that level and above) |
| `search` | string | — | Search in log messages |
| `start_date` | string | — | Start date filter (ISO 8601 format) |
| `end_date` | string | — | End date filter (ISO 8601 format) |

#### Example Requests

```bash
# Get recent logs (default: 50 records)
curl -H "Authorization: Bearer <admin_token>" \
  http://localhost:7070/api/v1/sys/logs

# Next page using cursor
curl -H "Authorization: Bearer <admin_token>" \
  "http://localhost:7070/api/v1/sys/logs?limit=50&after=<cursor>"

# Filter by log level
curl -H "Authorization: Bearer <admin_token>" \
  "http://localhost:7070/api/v1/sys/logs?level=warn"

# Get all errors and warnings (min_level)
curl -H "Authorization: Bearer <admin_token>" \
  "http://localhost:7070/api/v1/sys/logs?min_level=warn"

# Search in log messages
curl -H "Authorization: Bearer <admin_token>" \
  "http://localhost:7070/api/v1/sys/logs?search=database"

# Filter by date range
curl -H "Authorization: Bearer <admin_token>" \
  "http://localhost:7070/api/v1/sys/logs?start_date=2024-01-01T00:00:00Z&end_date=2024-01-31T23:59:59Z"

# Combined filters
curl -H "Authorization: Bearer <admin_token>" \
  "http://localhost:7070/api/v1/sys/logs?min_level=warn&search=error&limit=20"
```

#### Response Format

```json
{
  "data": {
    "items_count": 50,
    "limit": 50,
    "has_more": true,
    "cursor": "log_id_abc123",
    "items": [
      {
        "id": "log_id_123",
        "timestamp": "2024-01-15T10:30:45Z",
        "level": "warn",
        "origin": "entitySchema",
        "message": "Foreign key validation warning",
        "details": "Additional details about the log entry",
        "created_at": "2024-01-15T10:30:45Z"
      }
    ]
  }
}
```

Pass the returned `cursor` value as the `after` parameter in subsequent requests to page through results. When `has_more` is `false`, you have reached the last page.

#### Log Levels

Log levels in order of severity (lowest to highest):

1. `trace` - Detailed debugging information
2. `debug` - General debugging information
3. `info` - Informational messages
4. `warn` - Warning messages
5. `critical` - Critical errors

When using `min_level`, all logs at that level and above are included. For example, `min_level=warn` includes `warn` and `critical` logs.

#### Error Responses

**503 Service Unavailable** - Log database not initialized:
```json
{
  "error": "Log database not initialized",
  "status": 503,
  "data": {}
}
```

**500 Internal Server Error** - Server error:
```json
{
  "error": "Failed to fetch logs: <error message>",
  "status": 500,
  "data": {}
}
```

---

## Realtime API

MantisBase provides **realtime database change notifications** over **Server-Sent Events (SSE)** for both SQLite and PostgreSQL backends. Clients subscribe to topics (entity names and optionally specific row IDs) and receive live `insert`, `update`, and `delete` events as they occur.

### Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/realtime` | Open an SSE connection (optional initial `topics`). Returns `client_id` in the `connected` event. |
| POST | `/api/v1/realtime` | Set topics for an existing session. Returns granted topics and a `denied` list. |

### Authentication

Realtime connections accept optional credentials via `?token=` (overrides header) or `Authorization: Bearer <token>`. Invalid or expired tokens at connect time are treated as **guest**. Auth can be upgraded on subscribe (POST or WS `subscribe` message) but cannot be downgraded or switched to another user.

Session IDs use the prefixes `rt_sse_...` and `rt_ws_...`.

Connect-only sessions with no subscribed topics are closed after **60 seconds**. Authenticated sessions expire when the JWT/session is revoked or expires; idle subscribed sessions are closed after **10 minutes**.

### GET /api/v1/realtime — Open SSE connection

Establishes a long-lived SSE stream. Topics are optional on connect; use POST to subscribe after receiving `client_id`.

**Query parameters**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `topics` | string | No | Comma-separated list of topics. Each topic is an entity name (e.g. `posts`) or `entity:row_id`. Granted topics are filtered by access rules; denied topics appear in the `connected` event when present. |
| `token` | string | No | JWT or API key. Overrides `Authorization` when both are set. |

**Example (connect only)**

```bash
curl -N "http://localhost:7070/api/v1/realtime"
```

**Example (connect with public topics)**

```bash
curl -N "http://localhost:7070/api/v1/realtime?topics=posts,users"
```

**Response**

- **Content-Type:** `text/event-stream`
- **Connection:** keep-alive

The stream sends events in SSE format. Each event has an `event` type and a `data` line (JSON).

### POST /api/v1/realtime — Subscribe / update session

Sets the topic list for an existing SSE session (replaces prior subscriptions). Pass optional auth to upgrade from guest.

**Request body (JSON)**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `client_id` | string | Yes | Session ID from the `connected` event (`rt_sse_...`). |
| `topics` | array | Yes | Topics to subscribe to. Empty array clears subscriptions. |
| `token` | string | No | JWT or API key for auth upgrade (alternative to `Authorization` header). |

**Response (200)**

```json
{
  "client_id": "rt_sse_1769987962000_0abc1",
  "topics": ["posts"],
  "denied": [
    { "topic": "private_items", "reason": "forbidden", "status": 403 }
  ]
}
```

Auth downgrade or switching to another user returns **403**.

**Example**

```bash
curl -X POST http://localhost:7070/api/v1/realtime \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "client_id": "rt_sse_1769987962000_0abc1",
    "topics": ["posts", "comments"]
  }'
```

### SSE event types

| Event | Description |
|-------|-------------|
| `connected` | Sent once when the SSE connection is established. Contains `client_id`, `topics`, and `timestamp`. |
| `ping` | Keep-alive sent periodically (every ~30 s). Contains `timestamp`. |
| `change` | A database change (insert, update, or delete) for a subscribed topic. |
| `error` | Session error (e.g. `token_expired`). Connection may close afterward. |

### Event data format

**connected**

```json
{
  "client_id": "rt_sse_1769987962000_0abc1",
  "topics": ["posts", "users"],
  "timestamp": 1769987962
}
```

**ping**

```json
{"timestamp": 1769988043}
```

**change**

| Field | Type | Description |
|-------|------|-------------|
| `action` | string | One of `insert`, `update`, `delete`. |
| `entity` | string | Entity (table) name. |
| `row_id` | string | ID of the affected row. |
| `topic` | string | Topic that matched (entity or `entity:row_id`). |
| `timestamp` | number | Unix timestamp of the change. |
| `data` | object \| null | For `insert` and `update`, the row payload; for `delete`, `null`. Password fields on auth entities are omitted. |

**Example change (insert)**

```json
{
  "action": "insert",
  "data": {
    "created": "2026-02-02T02:19:38",
    "id": "019c1b81-364b-7000-8120-b5416b2c42c2",
    "updated": "2026-02-02T02:19:38"
  },
  "entity": "test",
  "row_id": "019c1b81-364b-7000-8120-b5416b2c42c2",
  "timestamp": 1769988013,
  "topic": "test"
}
```

**Example change (delete)**

```json
{
  "action": "delete",
  "data": null,
  "entity": "test",
  "row_id": "019c1b81-1501-7000-9d65-1541c14f99b7",
  "timestamp": 1769988013,
  "topic": "test"
}
```

### Access control

Realtime endpoints use the same access rules as entity `list` and `get`:

- Subscribing to an entity (e.g. `posts`) requires **list** access on that entity.
- Subscribing to a specific row (e.g. `posts:&lt;id&gt;`) requires **get** access.

Invalid or unauthorized topics result in `400`, `401`, or `403` responses depending on whether the request is malformed, unauthenticated, or forbidden by access rules.

### Backend support

Realtime is supported for:

- **SQLite** — Change detection via polling.
- **PostgreSQL** — Change detection via `LISTEN`/`NOTIFY` and triggers.

---

## WebSocket Realtime API

In addition to SSE, MantisBase provides a **WebSocket endpoint** for realtime notifications. It uses the same topic model as SSE.

### Endpoint

```
WS /api/v1/realtime/ws
```

### Connection

Authentication is **optional** at connect. Supply a JWT or API key (`mb_sk_...`) via `?token=` (overrides header) or `Authorization: Bearer <token>`. Invalid tokens are treated as guest. You receive a `client_id` immediately and subscribe afterward.

```javascript
const ws = new WebSocket("ws://localhost:7070/api/v1/realtime/ws");

ws.onopen = () => {
  // guest connect; pass token in subscribe to upgrade auth
  ws.send(JSON.stringify({
    type: "subscribe",
    token: token,
    topics: ["posts", "users"]
  }));
};

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  console.log(msg);
};
```

### Welcome message

On connect, the server sends:

```json
{
  "type": "connected",
  "client_id": "rt_ws_1769987962000_0abc1",
  "topics": []
}
```

### Subscribing

Send `{ "type": "subscribe", "topics": [...] }`. Optional `token` upgrades auth (for browsers that cannot set headers after connect). `{ "type": "auth", "token": "..." }` is equivalent to a token-only upgrade.

Topics follow the same format as SSE — entity name or `entity:row_id`. Subscribing **replaces** the current topic set.

```json
{ "type": "subscribe", "topics": ["posts", "comments"], "token": "<jwt>" }
```

The `subscribed` acknowledgement lists granted topics and denied failures:

```json
{
  "type": "subscribed",
  "topics": ["posts"],
  "denied": [{ "topic": "private_items", "reason": "forbidden", "status": 403 }]
}
```

### Change events

Database changes are delivered in the same format as SSE `change` events:

```json
{
  "action": "insert",
  "entity": "posts",
  "row_id": "019c1b81-364b-7000-8120-b5416b2c42c2",
  "topic": "posts",
  "timestamp": 1769988013,
  "data": { "id": "019c1b81-364b-7000-8120-b5416b2c42c2", "created": "2026-02-02T02:19:38" }
}
```

WebSocket realtime can be disabled by setting the environment variable `MB_DISABLE_REALTIME_WS=1` (or `true`). SSE can be disabled with `MB_DISABLE_REALTIME_SSE=1`.

---

## Admin Dashboard

The MantisBase Admin Dashboard is a comprehensive web-based interface accessible at `/mb` (e.g., `http://localhost:7070/mb`). It provides a visual alternative to the REST API for managing your backend.

### Dashboard Features

#### Entity Management
- **Browse Entities** - View all entities (tables) in your database
- **View Records** - Browse, search, and filter records in any entity
- **Create Records** - Add new records through intuitive forms
- **Edit Records** - Update existing records inline
- **Delete Records** - Remove records with confirmation

#### Schema Management
- **Schema Builder** - Create and configure entity schemas visually
- **Field Management** - Add, edit, and remove fields with type selection
- **Access Rules Configuration** - Set up access control rules with a user-friendly interface
- **Foreign Key Setup** - Configure relationships between entities
- **View Entity Support** - Create and manage SQL view entities

#### Data Exploration
- **Search & Filter** - Quickly find records with built-in search
- **Pagination** - Navigate through large datasets
- **Sorting** - Sort records by any column
- **Real-time Updates** - See changes reflected immediately

#### User Management
- **Authentication Entities** - Manage auth-type entities
- **User Accounts** - View and manage user accounts
- **Admin Accounts** - Manage admin users

#### System Management
- **Logs Viewer** - Access system logs directly from the dashboard
- **Health Status** - Monitor system health
- **Configuration** - View and manage system settings

### Accessing the Dashboard

1. **Create an Admin Account** (if not already created):
   ```bash
   ./mantisbase admins --add admin@example.com your_password
   ```

2. **Navigate to Dashboard**:
   ```
   http://localhost:7070/mb
   ```

3. **Login** with your admin credentials

### Dashboard Requirements

- **Admin Authentication Required** - Only users authenticated as admins can access the dashboard
- **Modern Browser** - Works best with Chrome, Firefox, Safari, or Edge (latest versions)
- **JavaScript Enabled** - The dashboard requires JavaScript to function

---
