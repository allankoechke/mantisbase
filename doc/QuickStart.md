@mainpage Quick Start Guide

<p align="center">
  <img src="assets/mantisbase-banner.jpg" alt="MantisBase Cover" width="100%" />
</p>

<p align="center">
  <strong>A lightweight, pluggable Backend-as-a-Service (BaaS) library built in C++</strong><br />
  Portable. Embeddable. Built for speed and extensibility.
</p>

---

## What is MantisBase?

MantisBase is a C++ library that provides a complete backend solution. Create database tables and instantly get REST APIs, authentication, file handling, and an admin dashboard - all in a single lightweight binary.

**Key Features:**
- Auto-generated REST APIs for all your data
- Built-in JWT authentication
- Access control with flexible rules
- Admin dashboard for data management
- File upload and serving
- JavaScript extensions for custom logic
- Embeddable in your C++ applications

---

## Installation

### Quick Start (Pre-built Binaries)

1. **Download** the latest release from [GitHub Releases](https://github.com/allankoechke/mantisbase/releases)
2. **Extract** the zip file
3. **Run** the server:

```bash
./mantisbase serve
```

The server starts on `http://localhost:7070`.

**Linux Dependencies:**
```bash
sudo apt-get install -y libzstd-dev libpq-dev
```

### Build from Source

```bash
git clone --recurse-submodules https://github.com/allankoechke/mantisbase.git
cd mantisbase
cmake -B build
cmake --build build
./build/mantisbase serve
```

See [Installation Guide](00.installation.md) for more details.

---

## First Steps

### 1. Create an Admin Account

Before using the admin dashboard, create an admin user:

```bash
./mantisbase admins --add admin@example.com admin_password_bjb12!
```


### 2. Access the Admin Dashboard

Open your browser and go to:

```
http://localhost:7070/mb
```

Log in with the admin credentials you just created.

### 3. Create Your First Entity

You can create entities (tables) via the admin dashboard or using the API:

**Using the Admin Dashboard:**
1. Log in to the admin dashboard
2. Navigate to "Schemas" or "Tables"
3. Click "Create New"
4. Define your table name, fields, and access rules

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
      {"name": "content", "type": "string"},
      {"name": "author_id", "type": "string", "required": true}
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

Once created, your entity automatically has REST endpoints:

```bash
# List all posts
curl http://localhost:7070/api/v1/entities/posts

# Get a specific post
curl http://localhost:7070/api/v1/entities/posts/<id>

# Create a post (requires authentication)
curl -X POST http://localhost:7070/api/v1/entities/posts \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"title": "My First Post", "content": "Hello World!", "author_id": "user123"}'

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

### User Login

MantisBase provides standalone authentication endpoints:

```bash
# Login
curl -X POST http://localhost:7070/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "entity": "users",
    "email": "user@example.com",
    "password": "password"
  }'

# Response: {"token": "eyJhbGc...", "user": {...}}
```

### Using Tokens

Include the token in all authenticated requests:

```bash
curl -H "Authorization: Bearer <token>" \
  http://localhost:7070/api/v1/entities/posts
```

See [Authentication API](02.auth.md) for all auth endpoints.

---

## Access Control

Each entity can define access rules for each operation:

- **`public`** - Open to everyone
- **`auth`** - Any authenticated user
- **Empty mode** - Admin only
- **`custom`** - JavaScript expression evaluation

Example rules:

```json
{
  "rules": {
    "list": {"mode": "public", "expr": ""},
    "get": {"mode": "public", "expr": ""},
    "add": {"mode": "auth", "expr": ""},
    "update": {"mode": "custom", "expr": "auth.id == req.body.author_id"},
    "delete": {"mode": "", "expr": ""}
  }
}
```

See [Access Rules](03.rules.md) for detailed documentation.

---

## File Handling

Upload files using multipart/form-data:

```bash
curl -X POST http://localhost:7070/api/v1/entities/posts \
  -H "Authorization: Bearer <token>" \
  -F "title=My Post" \
  -F "image=@photo.jpg"
```

Access files at:

```
http://localhost:7070/api/files/posts/photo.jpg
```

See [File Handling](11.files.md) for details.

---

## API Endpoints Overview

### Entity Endpoints

All entities automatically get these endpoints:

- `GET /api/v1/entities/<entity>` - List records
- `GET /api/v1/entities/<entity>/:id` - Get record
- `POST /api/v1/entities/<entity>` - Create record
- `PATCH /api/v1/entities/<entity>/:id` - Update record
- `DELETE /api/v1/entities/<entity>/:id` - Delete record

### Authentication Endpoints

- `POST /api/v1/auth/login` - User login
- `POST /api/v1/auth/refresh` - Refresh token
- `POST /api/v1/auth/logout` - Logout
- `POST /api/v1/auth/setup/admin` - Create initial admin

### Schema Management (Admin Only)

- `GET /api/v1/schemas` - List all schemas
- `GET /api/v1/schemas/:id` - Get schema
- `POST /api/v1/schemas` - Create schema
- `PATCH /api/v1/schemas/:id` - Update schema
- `DELETE /api/v1/schemas/:id` - Delete schema

### Other Endpoints

- `GET /api/v1/health` - Health check
- `GET /api/files/<entity>/<filename>` - Serve files
- `GET /mb` - Admin dashboard

See [API Reference](02.api.md) for complete documentation.

---

## Configuration

### Command-Line Options

```bash
# Custom port and host
mantisbase serve --port 8080 --host 0.0.0.0

# Development mode
mantisbase --dev serve

# PostgreSQL database
mantisbase --database PSQL \
  --connection "dbname=mantis host=localhost user=postgres password=pass" \
  serve

# Custom directories
mantisbase --dataDir ./data --publicDir ./public --scriptsDir ./scripts serve
```

### Environment Variables

```bash
# Set JWT secret (important for production)
export MANTIS_JWT_SECRET=your-secret-key-here
```

See [CLI Reference](01.cmd.md) for all options.

---

## Next Steps

Now that you have MantisBase running:

1. **Explore the Admin Dashboard** - Create tables, manage data, and configure access rules
2. **Read the API Documentation** - Learn about all available endpoints
3. **Set Up Access Rules** - Configure who can access what
4. **Try File Uploads** - Upload and serve files
5. **Add Custom Endpoints** - Extend functionality with JavaScript or C++
6. **Embed in Your App** - Use MantisBase as a library in your C++ project

---

## Documentation

- [Installation Guide](00.installation.md) - Detailed installation instructions
- [CLI Reference](01.cmd.md) - All command-line options
- [API Reference](02.api.md) - Complete API documentation
- [Authentication API](02.auth.md) - Auth endpoints and usage
- [Access Rules](03.rules.md) - Permission system guide
- [Embedding Guide](05.embedding.md) - Use as a C++ library
- [File Handling](11.files.md) - File upload and serving
- [Scripting Guide](13.scripting.md) - JavaScript extensions
- [Docker Guide](06.docker.md) - Running in containers

---

## Common Use Cases

### Desktop Application Backend

Embed MantisBase in your Qt, Slint, or native C++ desktop app to provide local data storage and REST APIs.

### Development Server

Use MantisBase as a quick backend for frontend development, prototyping, or testing.

### Embedded Device

Run MantisBase on embedded devices to provide a local API server with data persistence.

### Microservice

Deploy MantisBase as a standalone microservice for specific data management needs.

---

## Getting Help

- **Documentation**: Check the [docs directory](.) for detailed guides
- **Issues**: Report bugs or request features on [GitHub Issues](https://github.com/allankoechke/mantisbase/issues)
- **Examples**: See the [examples directory](../examples) for code samples

---

## Summary

MantisBase provides everything you need for a backend in a single C++ library:
- Create tables and get instant REST APIs
- Built-in authentication and access control
- Admin dashboard for easy management
- File handling and JavaScript extensions
- Embeddable in your applications

Get started in minutes, scale as needed.
