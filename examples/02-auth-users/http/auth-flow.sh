#!/usr/bin/env bash
# Auth flow: register → login → verify → refresh → logout
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:7070}"
EMAIL="${EMAIL:-demo@example.com}"
PASSWORD="${PASSWORD:-SecurePass123!}"
NAME="${NAME:-Demo User}"

echo "=== Register user (public) ==="
curl -sS -X POST "$BASE_URL/api/v1/entities/users" \
  -H "Content-Type: application/json" \
  -d "{\"name\": \"$NAME\", \"email\": \"$EMAIL\", \"password\": \"$PASSWORD\"}" \
  -w "\nHTTP %{http_code}\n"

echo "=== Login ==="
LOGIN=$(curl -sS -X POST "$BASE_URL/api/v1/auth/users/login" \
  -H "Content-Type: application/json" \
  -d "{\"identity\": \"$EMAIL\", \"password\": \"$PASSWORD\"}")
echo "$LOGIN"
TOKEN=$(echo "$LOGIN" | grep -o '"token":"[^"]*"' | head -1 | cut -d'"' -f4)

if [[ -z "$TOKEN" ]]; then
  echo "Login failed — check credentials or schema."
  exit 1
fi

echo "=== Verify token ==="
curl -sS "$BASE_URL/api/v1/auth/verify" \
  -H "Authorization: Bearer $TOKEN" \
  -w "\nHTTP %{http_code}\n"

echo "=== Refresh token ==="
REFRESH=$(curl -sS -X POST "$BASE_URL/api/v1/auth/users/refresh" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json")
echo "$REFRESH"
NEW_TOKEN=$(echo "$REFRESH" | grep -o '"token":"[^"]*"' | head -1 | cut -d'"' -f4)
TOKEN="${NEW_TOKEN:-$TOKEN}"

echo "=== Logout ==="
curl -sS -X POST "$BASE_URL/api/v1/auth/users/logout" \
  -H "Authorization: Bearer $TOKEN" \
  -w "\nHTTP %{http_code}\n"

echo "=== Invalid refresh (expect 401) ==="
curl -sS -o /dev/null -w "HTTP %{http_code}\n" -X POST "$BASE_URL/api/v1/auth/users/refresh" \
  -H "Authorization: Bearer invalid_token"

echo "USER_TOKEN=$TOKEN (export for other examples)"
