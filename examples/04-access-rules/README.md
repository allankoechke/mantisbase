# Access rules

Demonstrates **public**, **auth**, **custom**, and **admin-only** modes plus auth entity filtering.

## Apply schemas

```bash
export BASE_URL=http://localhost:7070
export ADMIN_TOKEN=<your_admin_jwt>

curl -X POST "$BASE_URL/api/v1/schemas" \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d @schemas/posts-owner-only.json

curl -X POST "$BASE_URL/api/v1/schemas" \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d @schemas/multi-role-users.json
```

## Scenarios

### posts-owner-only

| Operation | Mode | Behavior |
|-----------|------|----------|
| list / get | public | Anyone |
| add | auth | Logged-in users |
| update | custom | Only if `auth.id == req.body.author_id` |
| delete | admin-only (`mode: ""`) | Admins only |

### multi-role-users (editors entity)

Uses `auth` mode with `entity` filter on list/get — only users from the `editors` auth table pass.

## Edge cases to test manually

```bash
# Guest list posts — 200
curl "$BASE_URL/api/v1/entities/posts"

# Guest create post — 401
curl -X POST "$BASE_URL/api/v1/entities/posts" \
  -H "Content-Type: application/json" \
  -d '{"title": "x", "author_id": "fake"}'

# User update with wrong author_id — 403
curl -X PATCH "$BASE_URL/api/v1/entities/posts/<id>" \
  -H "Authorization: Bearer $USER_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"title": "Hacked", "author_id": "someone-else"}'
```

## Next steps

- [Access Rules](../../doc/rules.md) — expression reference and evaluation order
