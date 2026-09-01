# Realtime (SSE)

Subscribe to entity change events via Server-Sent Events.

## Apply schema

```bash
export BASE_URL=http://localhost:7070
export ADMIN_TOKEN=<your_admin_jwt>

curl -X POST "$BASE_URL/api/v1/schemas" \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d @schemas/messages.json
```

## Subscribe

Open an SSE connection (requires `curl` 7.86+ for `--no-buffer`):

```bash
curl -N "$BASE_URL/api/v1/realtime?topics=messages"
```

In another terminal, create a record:

```bash
curl -X POST "$BASE_URL/api/v1/entities/messages" \
  -H "Content-Type: application/json" \
  -d '{"title": "Hello realtime"}'
```

You should receive a `connected` event with `client_id`, then change events when records are created/updated/deleted.

## Expected behavior

| Scenario | Expected |
|----------|----------|
| Connect without topics | 200, empty topics array |
| Connect with `topics=messages` (public entity) | 200, `messages` in granted topics |
| Invalid topic name | 400 |
| Auth-only entity without token | Topic not granted |

See [`http/sse-subscribe.sh`](http/sse-subscribe.sh) for a minimal subscribe script.

## Next steps

- [API Reference — Realtime](../../doc/api.md)
- [05-realtime vs WebSocket](../../doc/api.md) — WS at `/api/v1/realtime/ws`
