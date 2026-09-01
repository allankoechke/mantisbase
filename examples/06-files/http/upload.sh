#!/usr/bin/env bash
# Upload a document with multipart form data
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:7070}"
FILE="${1:-}"

if [[ -z "$FILE" || ! -f "$FILE" ]]; then
  echo "Usage: $0 /path/to/file"
  exit 1
fi

AUTH=()
if [[ -n "${USER_TOKEN:-}" ]]; then
  AUTH=(-H "Authorization: Bearer $USER_TOKEN")
fi

echo "=== Upload document ==="
RESP=$(curl -sS -X POST "$BASE_URL/api/v1/entities/documents" \
  "${AUTH[@]}" \
  -F "title=Sample Document" \
  -F "attachment=@$FILE")
echo "$RESP"

FILENAME=$(echo "$RESP" | grep -o '"attachment":"[^"]*"' | head -1 | cut -d'"' -f4 || true)
if [[ -n "$FILENAME" ]]; then
  echo "=== Fetch file ==="
  curl -sS -o /dev/null -w "HTTP %{http_code}\n" "$BASE_URL/api/v1/files/documents/$FILENAME"
fi
