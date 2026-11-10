#!/usr/bin/env bash
# Email OTP login. SMTP must be configured or the request returns 400.
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:7070}"
ENTITY="${ENTITY:-users}"
EMAIL="${EMAIL:-demo@example.com}"

echo "=== Request sign-in code ==="
curl -sS -X POST "$BASE_URL/api/v1/auth/$ENTITY/otp/request" \
  -H "Content-Type: application/json" \
  -d "{\"email\": \"$EMAIL\"}" \
  -w "\nHTTP %{http_code}\n"

if [[ -z "${OTP:-}" ]]; then
  echo "Check the inbox for $EMAIL, then re-run with OTP=123456."
  exit 0
fi

echo "=== Sign in with code ==="
LOGIN=$(curl -sS -X POST "$BASE_URL/api/v1/auth/$ENTITY/otp/login" \
  -H "Content-Type: application/json" \
  -d "{\"email\": \"$EMAIL\", \"otp\": \"$OTP\"}")
echo "$LOGIN"
TOKEN=$(echo "$LOGIN" | grep -o '"token":"[^"]*"' | head -1 | cut -d'"' -f4)

if [[ -z "$TOKEN" ]]; then
  echo "OTP login failed."
  exit 1
fi

echo "USER_TOKEN=$TOKEN"
