# File uploads

Entity with `file` field — upload via multipart/form-data.

## Apply schema

```bash
export BASE_URL=http://localhost:7070
export ADMIN_TOKEN=<your_admin_jwt>

curl -X POST "$BASE_URL/api/v1/schemas" \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d @schemas/documents.json
```

## Upload

```bash
export BASE_URL=http://localhost:7070
export USER_TOKEN=<optional_if_public_add>
./http/upload.sh /path/to/sample.pdf
```

Files are stored under `<dataDir>/files/documents/` and served at:

```
GET /api/v1/files/documents/<filename>
```

## Expected behavior

| Action | Expected |
|--------|----------|
| Create with file field | 201, filename in response |
| Fetch file by URL | 200 with file bytes |
| Path traversal in filename | Sanitized/rejected |

## Next steps

- [File Handling](../../doc/files.md)
