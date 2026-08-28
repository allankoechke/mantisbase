#!/usr/bin/env bash
# SSE subscribe to messages entity. Run create in another terminal to see events.
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:7070}"
TOPICS="${TOPICS:-messages}"

echo "Connecting to SSE (Ctrl+C to stop)..."
echo "In another terminal: curl -X POST $BASE_URL/api/v1/entities/messages -H 'Content-Type: application/json' -d '{\"title\":\"ping\"}'"
echo

curl -N -sS "$BASE_URL/api/v1/realtime?topics=$TOPICS"
