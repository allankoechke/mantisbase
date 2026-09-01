#!/usr/bin/env bash
# E-commerce: products, user registration, orders with FK validation
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:7070}"

ADMIN_HDR=()
if [[ -n "${ADMIN_TOKEN:-}" ]]; then
  ADMIN_HDR=(-H "Authorization: Bearer $ADMIN_TOKEN")
fi

echo "=== Create product (admin-only; set ADMIN_TOKEN) ==="
PRODUCT=$(curl -sS -X POST "$BASE_URL/api/v1/entities/products" \
  "${ADMIN_HDR[@]}" \
  -H "Content-Type: application/json" \
  -d '{"name": "Widget", "price": 19.99}' \
  -w "\nHTTP %{http_code}\n")
echo "$PRODUCT"
PRODUCT_ID=$(echo "$PRODUCT" | grep -o '"id":"[^"]*"' | head -1 | cut -d'"' -f4 || true)

echo "=== Missing required field (expect 400) ==="
curl -sS -o /dev/null -w "HTTP %{http_code}\n" -X POST "$BASE_URL/api/v1/entities/products" \
  "${ADMIN_HDR[@]}" \
  -H "Content-Type: application/json" \
  -d '{"price": 9.99}'

echo "=== Register user ==="
USER=$(curl -sS -X POST "$BASE_URL/api/v1/entities/users" \
  -H "Content-Type: application/json" \
  -d '{"name": "Buyer", "email": "buyer@example.com", "password": "SecurePass123!"}')
echo "$USER"
USER_ID=$(echo "$USER" | grep -o '"id":"[^"]*"' | head -1 | cut -d'"' -f4 || true)

echo "=== Invalid email (expect 400) ==="
curl -sS -o /dev/null -w "HTTP %{http_code}\n" -X POST "$BASE_URL/api/v1/entities/users" \
  -H "Content-Type: application/json" \
  -d '{"name": "Bad", "email": "not-email", "password": "SecurePass123!"}'

if [[ -n "${USER_ID:-}" && -n "${PRODUCT_ID:-}" ]]; then
  echo "=== Login for order creation ==="
  LOGIN=$(curl -sS -X POST "$BASE_URL/api/v1/auth/users/login" \
    -H "Content-Type: application/json" \
    -d '{"identity": "buyer@example.com", "password": "SecurePass123!"}')
  TOKEN=$(echo "$LOGIN" | grep -o '"token":"[^"]*"' | head -1 | cut -d'"' -f4)

  echo "=== Create order ==="
  curl -sS -X POST "$BASE_URL/api/v1/entities/orders" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"label\": \"Order #1\", \"product_id\": \"$PRODUCT_ID\", \"user_id\": \"$USER_ID\"}" \
    -w "\nHTTP %{http_code}\n"
fi

echo "=== Order with bad user_id (expect 400) ==="
curl -sS -o /dev/null -w "HTTP %{http_code}\n" -X POST "$BASE_URL/api/v1/entities/orders" \
  -H "Content-Type: application/json" \
  -d '{"label": "Bad order", "user_id": "0000000000000000000"}'
