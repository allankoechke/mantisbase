@page files Handling Files in MantisBase

MantisBase supports file uploads and management for database records. Files can be associated with records through `file` or `files` field types and are stored on disk with references in the database.

---

## File Serving

All files stored in the database are accessible via:

```
GET /api/v1/files/<entity>/<filename>
```

- `<entity>`: The entity (table) name the file is associated with
- `<filename>`: The name of the file to retrieve

**Example:**

```bash
curl http://localhost:7070/api/v1/files/posts/image123.jpg
```

The endpoint returns the file if it exists, or a 404 error if not found.

---

## Creating Records with Files

To create a record with file uploads, send a POST request using `multipart/form-data`:

**Endpoint:** `POST /api/v1/entities/<entity>`

**Example using curl:**

```bash
curl -X POST http://localhost:7070/api/v1/entities/posts \
  -H "Authorization: Bearer <token>" \
  -F "title=My Post" \
  -F "content=Post content" \
  -F "image=@/path/to/image.jpg"
```

**Example using JavaScript (FormData):**

```javascript
const formData = new FormData();
formData.append('title', 'My Post');
formData.append('content', 'Post content');
formData.append('image', fileInput.files[0]);

fetch('http://localhost:7070/api/v1/entities/posts', {
  method: 'POST',
  headers: {
    'Authorization': 'Bearer ' + token
  },
  body: formData
});
```

The backend processes and stores the files, associating them with the new record. File names are stored in the database record.

---

## Updating Files in Records

To update files associated with a record, send a PATCH request with `multipart/form-data`:

**Endpoint:** `PATCH /api/v1/entities/<entity>/<id>`

**Example:**

```bash
curl -X PATCH http://localhost:7070/api/v1/entities/posts/123 \
  -H "Authorization: Bearer <token>" \
  -F "title=Updated Title" \
  -F "image=@/path/to/new-image.jpg"
```

**Important:** For fields that accept multiple files (`files` type), include all files you want to keep. Any files not included in the update will be deleted from both the database record and the filesystem.

---

## Deleting Files

To delete a file from a record:

1. Send a PATCH request to update the record
2. Omit the file name from the relevant field, or set it to `null`

**Example:**

```bash
# Update record without the file field
curl -X PATCH http://localhost:7070/api/v1/entities/posts/123 \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"title": "Updated", "image": null}'
```

The backend detects the missing file and removes it from both the database and filesystem.

---

## File Storage

Files are stored under a `files/` subdirectory inside the data directory:

```
<dataDir>/files/<entity>/<filename>
```

For example, if `dataDir` is `./data` and entity is `posts`, a file `image.jpg` would be stored at:

```
./data/files/posts/image.jpg
```

### Filename Sanitization

Uploaded filenames are automatically sanitized before storage:

- A random 8-character prefix is prepended (e.g., `a3b7c2d1_image.jpg`) to prevent collisions
- Invalid characters (control characters, `<>:"/\|?*+`, spaces, tabs, `%`, `=`) are replaced with underscores
- Consecutive underscores are collapsed
- Leading and trailing spaces or dots are trimmed
- Filenames longer than 255 characters are truncated with `...` in the middle
- If the result is empty after sanitization, the filename falls back to `"unnamed"`

### Path Traversal Protection

MantisBase uses multi-layered path traversal prevention on all file operations. Any request whose resolved path escapes the entity's storage directory is rejected with a `400 Bad Request` error.

### Limitations

- **File size**: A `maxFileSize` setting (default 10485760 bytes / 10 MiB) exists in the application config but is not currently enforced at upload time. Clients are not rejected for exceeding it.
- **File types**: No MIME type or file extension validation is performed. All file types are accepted.
- **Files per record**: No limit on the number of files per record.

---

## Field Types

MantisBase supports two file field types:

- **`file`** - Single file field
- **`files`** - Multiple files field (stored as JSON array of filenames)

When defining your schema:

```json
{
  "name": "posts",
  "fields": [
    {"name": "title", "type": "string"},
    {"name": "image", "type": "file"},
    {"name": "attachments", "type": "files"}
  ]
}
```

---

## Important Notes

- Always use the correct entity name and field names as defined in your schema
- For `files` type fields, include all files you want to retain in update requests
- File operations are transactional with record creation/updates
- Files are automatically deleted when records are deleted
- File names are sanitized to ensure filesystem safety
- **Runtime:** Set environment variable `MB_DISABLE_FILE_UPLOADS=1` to disable file uploads; the API will respond with `403 Forbidden` if a client attempts to upload files

