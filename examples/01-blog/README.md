# Blog (posts CRUD)

Minimal blog backend: a `posts` entity with public read and authenticated write.

## Apply schema

**Option A — API** (requires admin token):

```bash
export BASE_URL=http://localhost:7070
export ADMIN_TOKEN=<your_admin_jwt>

curl -X POST "$BASE_URL/api/v1/schemas" \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d @schemas/posts.json
```

**Option B — CLI migration:**

```bash
cp -r schemas ../migrations-tmp   # or point --migrations-dir at this folder
mantisbase --migrations-dir=./schemas migrate apply --up
```

## Run HTTP examples

```bash
export BASE_URL=http://localhost:7070
export USER_TOKEN=<token_from_auth_example_or_login>
./http/crud.sh
```

## Expected behavior

| Action | Auth | Expected status |
|--------|------|-----------------|
| List posts | None | 200 |
| Create post | User JWT | 201 |
| Create post | None | 401 |
| Update own post (matching `author_id`) | User JWT | 200 |
| Delete post | User JWT | 200 |
| Delete post | None | 401 |

## Next steps

- [Authentication API](../../doc/auth.md) — get a user token for writes
- [Access Rules](../../doc/rules.md) — customize the `rules` block in `schemas/posts.json`
