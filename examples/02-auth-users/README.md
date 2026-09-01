# Auth users

Auth-type entity with registration, login, token refresh, and logout.

## Apply schema

```bash
export BASE_URL=http://localhost:7070
export ADMIN_TOKEN=<your_admin_jwt>

curl -X POST "$BASE_URL/api/v1/schemas" \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d @schemas/users.json
```

## Run auth flow

```bash
export BASE_URL=http://localhost:7070
./http/auth-flow.sh
```

The script registers a user, logs in, verifies the token, refreshes it, and logs out.

## Expected behavior

| Action | Expected status |
|--------|-----------------|
| Register (public add) | 201 |
| Login with valid credentials | 200 (returns `data.token`) |
| Login with wrong password | 404 |
| Verify valid token | 200 |
| Refresh with valid token | 200 |
| Refresh with invalid token | 401 |
| Logout | 200 |

## Cookie auth

Login responses also set an HttpOnly `mb_token` cookie. Clients can call `/api/v1/auth/verify` with the cookie instead of a Bearer header.

## Next steps

- [Authentication API](../../doc/auth.md) — API keys, OAuth
- [01-blog](../01-blog/) — use the returned token for protected CRUD
