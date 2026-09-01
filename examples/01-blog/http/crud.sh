#!/usr/bin/env bash
# Blog CRUD example. Requires: BASE_URL, USER_TOKEN (optional ADMIN_TOKEN for schema setup).
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:7070}"

echo "=== List posts (public) ==="
curl -sS "$BASE_URL/api/v1/entities/posts" | head -c 500
echo -e "\n"

if [[ -z "${USER_TOKEN:-}" ]]; then
  echo "Set USER_TOKEN to run authenticated examples (see examples/02-auth-users)."
  exit 0
fi

echo "=== Create post ==="
CREATE=$(curl -sS -X POST "$BASE_URL/api/v1/entities/posts" \
  -H "Authorization: Bearer $USER_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"title": "Hello World", "content": "My first post"}')
echo "$CREATE"
POST_ID=$(echo "$CREATE" | grep -o '"id":"[^"]*"' | head -1 | cut -d'"' -f4)

if [[ -n "$POST_ID" ]]; then
  echo "=== Get post $POST_ID ==="
  curl -sS "$BASE_URL/api/v1/entities/posts/$POST_ID"
  echo -e "\n"

  echo "=== Update post ==="
  curl -sS -X PATCH "$BASE_URL/api/v1/entities/posts/$POST_ID" \
    -H "Authorization: Bearer $USER_TOKEN" \
    -H "Content-Type: application/json" \
    -d '{"title": "Updated Title"}'
  echo -e "\n"
fi

echo "=== Unauthenticated create (expect 401) ==="
curl -sS -o /dev/null -w "HTTP %{http_code}\n" -X POST "$BASE_URL/api/v1/entities/posts" \
  -H "Content-Type: application/json" \
  -d '{"title": "Should fail"}'
