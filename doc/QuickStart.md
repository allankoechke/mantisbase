
@mainpage Getting Started

<p align="center">
  <img src="assets/mantisbase-banner.jpg" alt="MantisBase Cover" width="100%" />
</p>

<p align="center">
  <strong>A lightweight, pluggable Backend-as-a-Service (BaaS) library built in C++</strong><br />
  Portable. Embeddable. Syncable. Built for speed and extensibility.
</p>

---

## 🔧 Overview

**MantisBase** is a modular, lightweight C++ library designed to power modern backend systems in embedded devices, desktop tools, or standalone server deployments. Inspired by systems like PocketBase and Supabase, MantisBase focuses on:

- Minimal runtime footprint
- SQLite as the default local database (with optional MySQL/PSQL support)
- Built-in authentication and access control
- Auto-generated REST APIs
- Pluggable sync layer (client-server)
- Embeddable as a reusable C++ library

---

## 🛠️ Tech Stack

- **Language**: C++
- **Database**: SQLite (default), MySQL/PSQL (planned)
- **Build System**: CMake
- **Packaging**: Docker + CLI
- **Sync**: WebSocket / REST delta sync (planned)

> **Note:** On Windows, use `mingw` (not `MSVC`) due to feature incompatibility. For `mingw`, version 13+ is required for `std::format` support.

---

## 🚀 Getting Started

There are several ways to get started with MantisBase:

### 1. Using Pre-built Binaries

Download pre-built binaries from the [release page](https://github.com/allankoechke/mantis/releases). Unzip the package and start the server:

```bash
mantisbase --dev serve
```

#### Creating an Admin Account
To use the admin dashboard, you must first create an admin user account. This can be done via the CLI:

```bash
mantisbase admins --add john@doe.com
```
You will be prompted to enter and confirm the password. The account can then be used to log in to the admin dashboard.

### 2. Building from Source

Clone the repository and build the project:

```bash
git clone --recurse-submodules https://github.com/allankoechke/mantisbase.git
cd mantisbase
cmake -B build
cmake --build build
./build/mantisbase serve
```
By default, the server runs on port `7070`.

### 3. Embedding in Another Project

You can embed MantisBase as a library in your own C++ project:

- Add this project as a submodule to your project.
- Link your project to the `mantisbase` library target.
- Extend your project as shown below:

```cpp
#include <mantis/mantisbase.h>

int main(const int argc, char* argv[])
{
    auto& app = mb::MantisBase::create(argc, argv);
    return app.run();
}
```
Check [mantis/examples](https://github.com/allankoechke/mantis/tree/master/examples) for a sample.

> **Note:** `MantisBase` has a blocking event loop when listening for HTTP events. To avoid blocking your main thread, run it in a separate thread if needed.

### 4. Using Docker

You can also run `mantisbase` in a Docker container. See [doc/docker.md](doc/docker.md) for more information.

---

## 🖥️ Admin Dashboard

MantisBase ships with a lightweight admin dashboard available at `<host>:<port>/admin` (admin login required). The dashboard allows for easy management of:

- **CRUD** on admin accounts
- **CRUD** on system logs [WIP]
- **CRUD** on database tables (managed by MantisBase)
- **CRUD** on records in the tables
- Schema & database migration [WIP]

By default, admin auth tokens expire after an hour (configurable in the dashboard settings).

![Mantis Admin](assets/mantis-admin.png)
_Admin dashboard snapshot_

---

## 📁 Project Structure

```
mantis/
├── include/
│   └── mantis/         # Public API headers
├── src/                # Internal implementation
├── examples/           # Embedding examples
├── tests/              # Unit & integration tests
├── docker/             # Docker deployment
└── CMakeLists.txt
```

---

## 🩺 Healthcheck Endpoint

MantisBase provides a healthcheck endpoint for monitoring and orchestration tools:

```
GET /api/v1/health
```

Returns:

```json
{
  "status": "ok",
  "uptime": 12345
}
```

Use this endpoint to verify server availability and uptime.

---

## 📂 File Handling

MantisBase supports file uploads and management via the API:

- **Serving files:**
  - All files stored in the database are served under `/api/files/<table name>/<filename>`
- **Creating records with files:**
  - Send a `POST` request to the table route using `multipart/form-data` (FormData)
- **Updating files in records:**
  - Send a `PATCH` request to the table record with new files as you would update other fields
  - For fields with multiple files, include the original files you want to keep; missing files will be deleted
- **Deleting files:**
  - Send an update (PATCH) omitting the file name from the field; the backend will delete the file

See [files.md](11.files.md) for more details.

---
# Extending MantisBase with JavaScript
MantisBase has a built-in lightweight JavaScript engine based on the `Duktape` library. This allows us to write scripts that will be evaluated to provide extensions to existing functionality.

For adding routes for instance:

```js
app.router().addRoute("GET", "/someapi/method/:arg", function(req, res){
    // Check if path params are available
    if(req.hasPathParam("arg")) {
        const arg = req.getPathParam("arg")
        const db_connected = app.db().connected
        res.json(200, {path: arg, connected: db_connected})
        return // Exit function to return response
    }

  // Or send response in this format
  res.send(500, JSON.stringify({error: "Dummy Error"}), "application/json")
})
```

Check more information at [Scripting Page](13.scripting.md).

---

## 🤝 Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](https://github.com/allankoechke/mantis/blob/master/CONTRIBUTING.md) and open an issue or PR.

