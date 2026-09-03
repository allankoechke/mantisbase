@page docs_embedding Embedding MantisBase

MantisBase is designed as a lightweight C++ library that can be embedded directly into your desktop, mobile, or embedded application. This guide explains how to integrate MantisBase into your project and configure it.

---

## Why Embed MantisBase?

- Add full-featured local storage and REST API support to your Qt, Slint, or CLI app
- Serve data to local UI components through HTTP or direct C++ API calls
- Retain full control of the application lifecycle and logic
- No separate server deployment required

---

## Dependencies

On Linux, building MantisBase requires **libpq** (PostgreSQL client) and **libuuid** development packages. These match what CI installs in `.github/workflows/build-matrix.yml`:

```shell
sudo apt-get update
sudo apt-get install -y libpq-dev uuid-dev
```

Runtime shared libraries on Debian/Ubuntu are `libpq5` and `libuuid1` (see `docker/Dockerfile`).

---

## Integration

### As a Static or Shared Library

Add MantisBase as a submodule or include it in your CMake project:

```cmake
add_subdirectory(mantisbase)
target_link_libraries(your_app PRIVATE mantisbase)
```

---

## Application lifecycle

`MantisBase::create()` returns a `std::unique_ptr<MantisBase>` that **you own**. There is no global `MantisBase::instance()` accessor. Hold the pointer in `main` (or your app object), pass `const MantisBase&` into your own code, and call `app->run()` to start the HTTP server.

Services that run inside MantisBase (`Entity`, `Router`, `MantisRequest`, `ApiKeyManager`, `OAuthManager`, etc.) inherit `IMantisBase` and expose `mbApp()` when you only have a request or service reference.

---

## Basic Usage

### Using Command-Line Arguments

The simplest way to embed MantisBase is using command-line arguments:

```cpp
#include <mantisbase/mantisbase.h>

int main(int argc, char* argv[])
{
    auto app = mb::MantisBase::create(argc, argv);
    return app->run();
}
```

This allows your application to accept standard MantisBase CLI arguments like `--db`, `--db_url`, `--dev`, and subcommands such as `serve --host` / `serve --port`.

### Using JSON Configuration

For more control when embedding, use JSON configuration:

```cpp
#include <mantisbase/mantisbase.h>

int main()
{
    mb::json config;
    config["dev"] = true;
    config["serve"] = {
        {"port", 8080},
        {"host", "127.0.0.1"}
    };
    config["db"] = "sqlite3";
    config["data-dir"] = "./data";
    config["public-dir"] = "./public";
    config["scripts-dir"] = "./scripts";
    
    auto app = mb::MantisBase::create(config);
    return app->run();
}
```

### Minimal Configuration

You can also create an instance with default settings:

```cpp
#include <mantisbase/mantisbase.h>

int main()
{
    // Uses all defaults (port 7070, SQLite, etc.)
    auto app = mb::MantisBase::create();
    return app->run();
}
```

---

## Accessing services (DI)

Reach auth, entities, and database APIs through your owned app instance:

```cpp
auto app = mb::MantisBase::create(config);

// JWT tokens
mb::json claims = {{"id", "user123"}, {"table", "users"}};
std::string jwt = app->auth().createToken(claims);

// API keys (raw secret returned once at creation)
auto key = app->auth().apiKey().create("users", "user123", "Desktop client");

// OAuth (admin setup + user flows)
app->auth().oauth().enableProviderForEntity("users", provider_id);

// Entities and database
auto users = app->entity("users");
auto sql = app->db().session();
```

Inside custom route handlers, use `req.mbApp()`:

```cpp
router.Get("/api/v1/custom", [](mb::MantisRequest& req, mb::MantisResponse& res) {
    auto count = req.mbApp().entity("posts").countRecords();
    res.sendJSON(200, {{"posts", count}});
});
```

Per-request data (`auth`, `verification`, etc.) is stored on `MantisRequest` via `set()` / `getOr()`, not a global singleton.

---

## Configuration Options

The JSON configuration supports the following options:

```json
{
  "db": "sqlite3",
  "db_url": "",
  "data-dir": "./data",
  "public-dir": "./public",
  "scripts-dir": "./scripts",
  "migrations-dir": "./migrations",
  "dev": true,
  "serve": {
    "port": 7070,
    "host": "127.0.0.1",
    "pool-size": 4,
    "skip-admin-setup": false
  },
  "admins": {
    "add": ["admin@example.com", "password"],
    "ls": false,
    "rm": "admin@example.com"
  },
  "migrations": {
    "up": true
  },
  "schema": {
    "ls": true
  }
}
```

All options are optional and will use sensible defaults if omitted.

---

## Adding Custom Routes

You can add custom API endpoints using the router:

```cpp
#include <mantisbase/mantisbase.h>
#include <mantisbase/core/middlewares.h>

int main()
{
    auto app = mb::MantisBase::create();
    auto& router = app->router();
    
    // Simple GET endpoint
    router.Get("/api/v2/custom", [](mb::MantisRequest& req, mb::MantisResponse& res) {
        res.sendJSON(200, {{"message", "Custom endpoint"}});
    });
    
    // POST endpoint with admin authentication
    router.Post("/api/v2/admin/stats", [](mb::MantisRequest& req, mb::MantisResponse& res) {
        mb::json stats = {{"users", 100}, {"posts", 500}};
        res.sendJSON(200, stats);
    }, {mb::requireAdminAuth()});
    
    // GET endpoint with entity authentication
    router.Get("/api/v2/users/profile", [](mb::MantisRequest& req, mb::MantisResponse& res) {
        auto auth = req.getOr<mb::json>("auth", mb::json::object());
        std::string userId = auth["id"];
        res.sendJSON(200, {{"user_id", userId}});
    }, {mb::requireEntityAuth("users")});
    
    return app->run();
}
```

---

## Accessing Database

You can access the database directly for custom operations:

```cpp
#include <mantisbase/mantisbase.h>
#include <soci/soci.h>

int main()
{
    auto app = mb::MantisBase::create();
    
    // Get a database session
    auto sql = app->db().session();
    
    // Execute queries
    soci::row row;
    *sql << "SELECT * FROM users WHERE id = :id", 
         soci::use("user123"), 
         soci::into(row);
    
    if (sql->got_data()) {
        // Process row data
    }
    
    return app->run();
}
```

---

## Working with Entities

You can interact with entities programmatically:

```cpp
#include <mantisbase/mantisbase.h>

int main()
{
    auto app = mb::MantisBase::create();
    
    // Get an entity (bound to the app instance)
    auto users = app->entity("users");
    
    // Create a record
    mb::json newUser = {
        {"name", "John Doe"},
        {"email", "john@example.com"}
    };
    auto created = users.create(newUser);
    
    // List records
    auto allUsers = users.list();
    
    // Read a record
    if (auto user = users.read("user123"); user.has_value()) {
        // Process user data
    }
    
    // Update a record
    mb::json updates = {{"name", "Jane Doe"}};
    auto updated = users.update("user123", updates);
    
    // Delete a record
    users.remove("user123");
    
    return app->run();
}
```

---

## Project Structure

A typical embedded project structure:

```
your-app/
├── main.cpp
├── CMakeLists.txt
├── data/              # SQLite database and files (created automatically)
├── public/            # Static assets (optional)
├── scripts/           # JavaScript extensions (optional)
└── mantisbase/        # MantisBase submodule
```

---

## Using as a Submodule

Add MantisBase as a git submodule:

```bash
git submodule add https://github.com/allankoechke/mantisbase.git
git submodule update --init --recursive
```

Then include it in your CMakeLists.txt:

```cmake
add_subdirectory(mantisbase)
target_link_libraries(your_app PRIVATE mantisbase)
```

---

## Testing Embedded APIs

Once your app is running, test the APIs:

```bash
# Test entity endpoints
curl http://localhost:7070/api/v1/entities/users

# Test custom endpoints
curl http://localhost:7070/api/v1/custom

# Test with JWT or API key
curl -H "Authorization: Bearer <token>" \
     http://localhost:7070/api/v1/entities/posts

curl -H "Authorization: Bearer mb_sk_..." \
     http://localhost:7070/api/v1/entities/posts
```

---

## Notes

- MantisBase APIs respect all access rules and authentication even in embedded mode
- The `run()` method is blocking - use a separate thread if you need non-blocking behavior
- All entity endpoints follow the pattern `/api/v1/entities/<entity_name>`
- Token verification: `GET /api/v1/auth/verify` (JWT or API key)
- Authentication endpoints are at `/api/v1/auth/<entity>/` (auth-type entities only)
- API keys and OAuth routes live under `/api/v1/auth/<entity>/api-keys` and `/api/v1/auth/<entity>/oauth/`
- Schema management endpoints are at `/api/v1/schemas/*` (admin only)
- File serving endpoints are at `/api/v1/files/*`
- System endpoints are at `/api/v1/sys/*` (logs, admins, settings, api-keys, oauth); health is at `/api/v1/health`

---

## Complete Example

```cpp
#include <mantisbase/mantisbase.h>
#include <mantisbase/core/middlewares.h>

int main()
{
    mb::json config;
    config["dev"] = true;
    config["serve"] = {{"port", 8080}};
    
    auto app = mb::MantisBase::create(config);
    auto& router = app->router();
    
    router.Get("/api/v1/hello", [](mb::MantisRequest& req, mb::MantisResponse& res) {
        res.sendJSON(200, {{"message", "Hello from embedded MantisBase!"}});
    });
    
    return app->run();
}
```

---

## Summary

By embedding MantisBase, you gain powerful backend features including database storage, JWT and API-key authentication, OAuth, and REST APIs without requiring a separate server deployment. Own the `MantisBase` instance in your process, inject `const MantisBase&` (or use `mbApp()` in handlers), and integrate with minimal configuration.
