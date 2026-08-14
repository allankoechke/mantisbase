# MantisBase for Linux

Prebuilt MantisBase server binary for Linux.

## Quick start

1. Extract this archive.
2. Run the server:

```bash
chmod +x mantisbase
./mantisbase serve
```

3. Open the admin dashboard at http://localhost:7070/mb and create your admin account, or use the CLI:

```bash
./mantisbase admins --add admin@example.com your_strong_password
```

The API is available at http://localhost:7070/api/v1/.

## Configuration

```bash
# Custom port and host
./mantisbase serve --port 8080 --host 0.0.0.0

# PostgreSQL instead of SQLite (default)
./mantisbase --db postgresql --db_url "dbname=mydb host=localhost user=postgres password=pass" serve

# Development mode (verbose logging)
./mantisbase --dev serve
```

Set `MB_JWT_SECRET` in production. See the [CLI Reference](https://github.com/allankoechke/mantisbase/blob/master/doc/cmd.md).

## Runtime dependencies (Linux)

This binary bundles most dependencies. If you use PostgreSQL, ensure client libraries are available on the host (`libpq5`, `libuuid1` on Debian/Ubuntu).

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
