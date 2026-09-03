@mainpage Quick Start Guide

<p align="center">
  <img src="assets/mantisbase-banner.jpg" alt="MantisBase Cover" width="100%" />
</p>

<p align="center">
  <strong>A self-hosted C++20 BaaS — one binary, instant REST APIs, auth, realtime, and an admin dashboard.</strong>
</p>

---

## Installation

### Pre-built Binary

1. Download the latest release from [GitHub Releases](https://github.com/allankoechke/mantisbase/releases)
2. Extract and run:

```bash
./mantisbase serve
```

The server starts on `http://localhost:7070`.

**Linux dependencies:**
```bash
sudo apt-get install -y libpq-dev uuid-dev
```

### Build from Source

```bash
git clone --recurse-submodules https://github.com/allankoechke/mantisbase.git
cd mantisbase
cmake -B build
cmake --build build
./build/mantisbase serve
```

See [Installation Guide](installation.md) for more details.

---

## First Steps

### 1. Create an Admin Account

```bash
./mantisbase admins --add admin@example.com your_password
```

### 2. Access the Admin Dashboard

Open `http://localhost:7070/mb` and log in with your admin credentials.

The dashboard lets you create entities, manage records and schemas, configure access rules, and upload files — all without writing API calls.

![MantisBase Admin Dashboard](assets/mantisbase-admin.png)

### 3. Create Your First Entity

**Using the Admin Dashboard (recommended):** navigate to Schemas, click New, and fill in the name, fields, and access rules.

**Using the API:**

```bash
curl -X POST http://localhost:7070/api/v1/schemas \
  -H "Authorization: Bearer <admin_token>" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "posts",
    "type": "base",
    "fields": [
      {"name": "title", "type": "string", "required": true},
      {"name": "content", "type": "string"}
    ],
    "rules": {
      "list": {"mode": "public", "expr": ""},
      "get": {"mode": "public", "expr": ""},
      "add": {"mode": "auth", "expr": ""},
      "update": {"mode": "custom", "expr": "auth.id == req.body.author_id"},
      "delete": {"mode": "", "expr": ""}
    }
  }'
```

### 4. Use Your Auto-generated API

```bash
# List all posts
curl http://localhost:7070/api/v1/entities/posts

# Get a specific post
curl http://localhost:7070/api/v1/entities/posts/<id>

# Create a post (requires authentication)
curl -X POST http://localhost:7070/api/v1/entities/posts \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"title": "My First Post", "content": "Hello World!"}'

# Update a post
curl -X PATCH http://localhost:7070/api/v1/entities/posts/<id> \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"title": "Updated Title"}'

# Delete a post
curl -X DELETE http://localhost:7070/api/v1/entities/posts/<id> \
  -H "Authorization: Bearer <token>"
```

---

## Authentication

```bash
# Login
curl -X POST http://localhost:7070/api/v1/auth/users/login \
  -H "Content-Type: application/json" \
  -d '{"identity": "user@example.com", "password": "password"}'

# Use the returned token
curl -H "Authorization: Bearer <token>" \
  http://localhost:7070/api/v1/entities/posts

# Verify the token is still valid
curl http://localhost:7070/api/v1/auth/verify \
  -H "Authorization: Bearer <token>"
```

See [Authentication API](auth.md) for verify, refresh, logout, API keys, and OAuth.

---

## Access Control

Each entity defines per-operation access rules:

| Mode | Description |
|------|-------------|
| `"public"` | Open to everyone |
| `"auth"` | Any authenticated user; optional `entity` filter restricts auth entity |
| `""` (empty) | Admin only |
| `"custom"` | JavaScript expression — e.g. `auth.id == req.body.author_id` |

See [Access Rules](rules.md) for details.

---

## File Uploads

```bash
curl -X POST http://localhost:7070/api/v1/entities/posts \
  -H "Authorization: Bearer <token>" \
  -F "title=My Post" \
  -F "image=@photo.jpg"
```

Serve files at `GET /api/v1/files/posts/<filename>`. See [File Handling](files.md).

---

## Configuration

```bash
# Custom port and host
mantisbase serve --port 8080 --host 0.0.0.0

# Development mode
mantisbase --dev serve

# PostgreSQL database
mantisbase --db postgresql \
  --db_url "dbname=mantis host=localhost user=postgres password=pass" \
  serve
```

Set `MB_JWT_SECRET` in production. See [CLI Reference](cmd.md) for all options.

---

## Next Steps

1. [API Reference](api.md) — All endpoints, schema management, realtime SSE and WebSocket
2. [Authentication API](auth.md) — Auth endpoints, API keys, OAuth
3. [Access Rules](rules.md) — Permission system
4. [File Handling](files.md) — Upload, serve, and delete files
5. [Embedding Guide](embedding.md) — Use MantisBase as a C++ library
6. [Scripting Guide](scripting.md) — JavaScript extensions for custom routes
7. [Docker Guide](docker.md) — Container deployment

---

## Documentation

- [Installation Guide](installation.md) — Detailed installation instructions
- [CLI Reference](cmd.md) — All command-line options
- [API Reference](api.md) — Complete API documentation
- [Authentication API](auth.md) — Auth endpoints and usage
- [Access Rules](rules.md) — Permission system guide
- [Embedding Guide](embedding.md) — Use as a C++ library
- [File Handling](files.md) — File upload and serving
- [Scripting Guide](scripting.md) — JavaScript extensions
- [Docker Guide](docker.md) — Running in containers
