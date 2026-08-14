# MantisBase for Windows

Prebuilt MantisBase server binary for Windows (64-bit, MinGW).

## Quick start

1. Extract this archive.
2. Open PowerShell or Command Prompt in the extracted folder.
3. Run the server:

```powershell
.\mantisbase.exe serve
```

4. Open the admin dashboard at http://localhost:7070/mb and create your admin account, or use the CLI:

```powershell
.\mantisbase.exe admins --add admin@example.com your_strong_password
```

The API is available at http://localhost:7070/api/v1/.

## Configuration

```powershell
# Custom port and host
.\mantisbase.exe serve --port 8080 --host 0.0.0.0

# Development mode (verbose logging)
.\mantisbase.exe --dev serve
```

Set `MB_JWT_SECRET` in production. See the [CLI Reference](https://github.com/allankoechke/mantisbase/blob/master/doc/cmd.md).

> **Note:** Prebuilt Windows binaries use SQLite by default. PostgreSQL backend support is included in Linux builds; build from source on Windows if you need PostgreSQL.

## Other install options

- **Docker:** `docker run -p 7070:80 allankoech/mantisbase`
- **Build from source:** https://github.com/allankoechke/mantisbase/blob/master/doc/installation.md

## Get help

- Quick Start: https://github.com/allankoechke/mantisbase/blob/master/doc/QuickStart.md
- Documentation: https://allankoechke.github.io/mantisbase/
- GitHub Discussions: https://github.com/allankoechke/mantisbase/discussions
- Issues: https://github.com/allankoechke/mantisbase/issues

## License

See `LICENSE` in this archive. MantisBase is released under the MIT License.
