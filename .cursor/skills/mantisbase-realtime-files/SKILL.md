---
name: mantisbase-realtime-files
description: >-
  MantisBase SSE realtime subscriptions, WebSocket, and file upload fields.
  Use when building live dashboards or document/attachment features.
---

# MantisBase Realtime & Files

## Server-Sent Events (SSE)

Connect:
```
GET /api/v1/realtime?topics=messages,posts
```

- First event: `event: connected` with `client_id` and granted `topics`.
- Subsequent events on record create/update/delete.
- Public entities grant topics to guests; auth-only entities require Bearer token.

Example: [`examples/05-realtime/`](../../examples/05-realtime/).

```bash
curl -N "http://localhost:7070/api/v1/realtime?topics=messages"
```

WebSocket alternative: `/api/v1/realtime/ws` (see API docs).

## File fields

Schema field types: `file` (single), `files` (multiple).

```json
{"name": "attachment", "type": "file"}
```

Upload via multipart:
```bash
curl -X POST http://localhost:7070/api/v1/entities/documents \
  -H "Authorization: Bearer $TOKEN" \
  -F "title=Report" \
  -F "attachment=@./report.pdf"
```

Serve files:
```
GET /api/v1/files/{entity}/{filename}
```

Example: [`examples/06-files/`](../../examples/06-files/).

## Notes

- Filenames are sanitized and prefixed to prevent collisions.
- PATCH with `files` type: include all files to keep; omitted files are deleted.
- Disable uploads in production if unused: `MB_DISABLE_FILE_UPLOADS=1`.

## Docs

- [API — Realtime](../../doc/api.md)
- [File Handling](../../doc/files.md)
