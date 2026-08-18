@page auth_api Authentication API

MantisBase provides authentication endpoints scoped to auth-type entities. Each auth entity exposes login, refresh, and logout under `/api/v1/auth/<entity>/`. A global token verification endpoint is available at `/api/v1/auth/verify`.

Admin authentication uses `/api/v1/sys/admins/` instead.

### HTTP status codes

| Code | Meaning |
|------|---------|
| `401 Unauthorized` | Missing, invalid, or expired credentials |
| `403 Forbidden` | Authenticated, but not allowed to perform the action (for example, a regular user token on an admin-only route) |

---

## Base URL

```
http://localhost:7070/api/v1/auth/<entity>/
```

`<entity>` must be a registered entity that is:

- Type `auth`
- Not a system entity (e.g. not `mb_admins`)
- API-enabled (`has_api: true`)

If the entity is missing or does not meet these requirements, the route returns **404 Route Not Found**.

---

## Authentication Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/auth/verify` | Check whether the current JWT or API key is still valid |
| POST | `/api/v1/auth/<entity>/login` | Authenticate user and get token |
| POST | `/api/v1/auth/<entity>/refresh` | Refresh an existing token |
| POST | `/api/v1/auth/<entity>/logout` | Logout (invalidate token) |

For admin authentication and initial setup, see [System Endpoints – Admin Accounts](api.md#admin-accounts) (`/api/v1/sys/admins/`).

---

## Login

Authenticate a user and receive a JWT token.

> ⚠️ **Rate Limiting**: Login is rate-limited to **5 attempts per minute per IP address** to prevent brute force attacks. If you exceed this limit, you'll receive a `429 Too Many Requests` response with a `Retry-After` header indicating when you can try again.

**Endpoint:** `POST /api/v1/auth/<entity>/login`

**Request Body:**
```json
{
  "identity": "user@example.com",
  "password": "userpassword"
}
```

**Request Fields:**
- `identity` (required): User identifier — email address or user ID
- `password` (required): User's password

The target entity is taken from the URL path, not the request body.

**Response (Success - 200):**
```json
{
  "status": 200,
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "user": {
      "id": "123456",
      "email": "user@example.com",
      "name": "John Doe"
    }
  },
  "error": ""
}
```

**Response (Error - 404):**
```json
{
  "status": 404,
  "data": {},
  "error": "No user found for given `identity`, `password` & `entity` combination."
}
```

**Response (Rate Limited - 429):**
```json
{
  "status": 429,
  "data": {},
  "error": "Rate limit exceeded. Maximum 5 requests per 60 seconds. Retry after 45 seconds."
}
```

The rate limit response includes these headers:
- `X-RateLimit-Limit`: Maximum requests allowed (5)
- `X-RateLimit-Remaining`: Remaining requests in window (0 when rate limited)
- `X-RateLimit-Reset`: Unix timestamp when the window resets
- `Retry-After`: Seconds to wait before retrying

**Examples:**

Login with email:
```bash
curl -X POST http://localhost:7070/api/v1/auth/users/login \
  -H "Content-Type: application/json" \
  -d '{
    "identity": "user@example.com",
    "password": "userpassword"
  }'
```

Login with user ID:
```bash
curl -X POST http://localhost:7070/api/v1/auth/users/login \
  -H "Content-Type: application/json" \
  -d '{
    "identity": "019b292a-e145-7000-813b-c9f528364a2b",
    "password": "userpassword"
  }'
```

---

## Refresh Token

Refresh an existing JWT token to extend its validity.

**Endpoint:** `POST /api/v1/auth/<entity>/refresh`

**Request Headers:**
```
Authorization: Bearer <existing_token>
```

**Response (Success - 200):**
```json
{
  "status": 200,
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "user": {
      "id": "123456",
      "email": "user@example.com"
    }
  },
  "error": ""
}
```

**Example:**
```bash
curl -X POST http://localhost:7070/api/v1/auth/users/refresh \
  -H "Authorization: Bearer <existing_token>"
```

---

## Logout

Logout and invalidate the current token.

**Endpoint:** `POST /api/v1/auth/<entity>/logout`

**Request Headers:**
```
Authorization: Bearer <token>
```

**Response (Success - 200):**
```json
{
  "status": 200,
  "data": {
    "logged_out": true
  },
  "error": ""
}
```

**Example:**
```bash
curl -X POST http://localhost:7070/api/v1/auth/users/logout \
  -H "Authorization: Bearer <token>"
```

---

## Verify Token

Check whether the current JWT or API key is still valid without performing a refresh or logout. Useful for client-side session checks and keeping UI auth state in sync.

> ⚠️ **Rate Limiting**: Verify is rate-limited to **5 requests per minute per IP address**. If you exceed this limit, you'll receive a `429 Too Many Requests` response.

**Endpoint:** `GET /api/v1/auth/verify`

This route is not entity-scoped. It accepts any valid entity user JWT or `mb_sk_...` API key via the standard `Authorization` header.

**Request Headers:**
```
Authorization: Bearer <token>
```

**Response (Success - 200):**
```json
{
  "status": 200,
  "data": {
    "status": "OK"
  },
  "error": ""
}
```

**Response (Missing token - 401):**
```json
{
  "status": 401,
  "data": {},
  "error": "Missing or invalid auth token"
}
```

**Response (Invalid or expired token - 401):**
```json
{
  "status": 401,
  "data": {},
  "error": "Token Verification Error"
}
```

The `error` field may contain a more specific message when token verification fails (for example, an expired JWT).

**Examples:**

Verify a JWT:
```bash
curl http://localhost:7070/api/v1/auth/verify \
  -H "Authorization: Bearer <token>"
```

Verify an API key:
```bash
curl http://localhost:7070/api/v1/auth/verify \
  -H "Authorization: Bearer mb_sk_..."
```

---

## Admin Setup and Authentication

Admin account setup and admin-only authentication use the `/api/v1/sys/admins/` namespace. See [System Endpoints – Admin Accounts](api.md#admin-accounts) in the API reference.

**Initial setup:** `POST /api/v1/sys/admins/setup` (only when no admin accounts exist)

**Admin login:** `POST /api/v1/sys/admins/login`

---

## Using Tokens

After receiving a token from the login endpoint, include it in all subsequent API requests:

```
Authorization: Bearer <token>
```

JWTs and API keys both use the same header. API keys are prefixed with `mb_sk_` and are validated by the same auth middleware as JWTs.

The token contains user information (id, entity) and is validated automatically by the `getAuthToken()` middleware on all endpoints.

---

## API Keys

Long-lived API keys complement short-lived JWTs. Keys are scoped to an auth entity and user, stored hashed server-side, and shown **once** at creation.

### Authentication

Send the raw key in the standard Authorization header:

```
Authorization: Bearer mb_sk_<secret>
```

The middleware resolves the key, hydrates the user record, and sets the same `auth` / `verification` request attributes as a JWT login.

### Entity user endpoints

Requires an authenticated session for the same auth entity (JWT from login).

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/auth/<entity>/api-keys` | Create a key (returns one-time `key`) |
| GET | `/api/v1/auth/<entity>/api-keys` | List keys (metadata only) |
| DELETE | `/api/v1/auth/<entity>/api-keys/:id` | Revoke a key |

**Create request body:**

```json
{
  "label": "Mobile app",
  "permissions": [],
  "expires_at": ""
}
```

**Example:**

```bash
# Create (requires JWT for the same entity)
curl -X POST http://localhost:7070/api/v1/auth/users/api-keys \
  -H "Authorization: Bearer <jwt>" \
  -H "Content-Type: application/json" \
  -d '{"label": "CI pipeline"}'

# Use the returned mb_sk_... key on API calls
curl -H "Authorization: Bearer mb_sk_..." \
  http://localhost:7070/api/v1/entities/posts
```

### Admin endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/sys/api-keys` | Create admin API key |
| GET | `/api/v1/sys/api-keys` | List admin keys |
| DELETE | `/api/v1/sys/api-keys/:id` | Revoke admin key |

Admin routes require admin JWT authentication.

### C++ API

```cpp
auto app = MantisBase::create();
auto& keys = app->auth().apiKey();
auto created = keys.create("users", user_id, "Desktop client");
// created["key"] — store securely; not returned again on list()
```

---

## OAuth

OAuth 2.0 / OIDC login and account linking for auth-type entities. Preset providers (`google`, `github`, `discord`, `microsoft`) are seeded at startup; admins configure client credentials and enable providers per entity.

### User-facing routes

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/auth/<entity>/oauth/authorize/:provider` | Redirect to provider (PKCE) |
| GET | `/api/v1/auth/<entity>/oauth/callback/:provider` | OAuth callback; returns JWT on success |
| POST | `/api/v1/auth/<entity>/oauth/link/:provider` | Link provider to logged-in user |
| DELETE | `/api/v1/auth/<entity>/oauth/link/:provider` | Unlink provider |
| GET | `/api/v1/auth/<entity>/oauth/accounts` | List linked accounts |
| GET | `/api/v1/auth/<entity>/oauth/providers` | Providers enabled for entity |

**Start login (browser redirect):**

```bash
curl -L "http://localhost:7070/api/v1/auth/users/oauth/authorize/google?redirect_uri=http://localhost:3000/callback"
```

### Admin provider management

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/v1/sys/oauth/providers` | Add provider (`name`, `client_id`, `client_secret`) |
| GET | `/api/v1/sys/oauth/providers` | List providers |
| PATCH | `/api/v1/sys/oauth/providers/:id` | Update provider |
| DELETE | `/api/v1/sys/oauth/providers/:id` | Remove provider |
| POST | `/api/v1/sys/oauth/entity-config` | Enable provider for entity |
| DELETE | `/api/v1/sys/oauth/entity-config` | Disable provider for entity |

**Enable provider for an entity:**

```bash
curl -X POST http://localhost:7070/api/v1/sys/oauth/entity-config \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{"entity_name": "users", "provider_id": "<provider_uuid>"}'
```

### C++ API

```cpp
auto app = MantisBase::create();
auto& oauth = app->auth().oauth();
oauth.addProvider({{"name", "google"}, {"client_id", "..."}, {"client_secret", "..."}});
oauth.enableProviderForEntity("users", provider_id);
auto url = oauth.buildAuthorizeUrl("users", "google", redirect_uri);
```

---

## Token Expiration

By default, tokens expire after 1 hour. Use the refresh endpoint to extend token validity without requiring the user to log in again.

---

## Summary

The authentication API provides JWT login, token verification (`GET /api/v1/auth/verify`), long-lived API keys (`mb_sk_...`), and OAuth for auth-type entities. All credential types use `Authorization: Bearer ...` and are validated before entity access rules run. Admin-only routes under `/api/v1/sys/` manage providers and system API keys.
