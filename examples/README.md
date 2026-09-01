# MantisBase Examples

Copy-paste schemas, migrations, HTTP scripts, and scripting samples for common use cases. Each folder is self-contained with a README explaining prerequisites and expected results.

**Prerequisites:** A running MantisBase server (`./mantisbase serve` or Docker). Create an admin account at `/mb` or via `./mantisbase admins --add admin@example.com your_password`.

## Quick index

| Example | What it demonstrates |
|---------|---------------------|
| [01-blog](01-blog/) | Basic entity schema + CRUD |
| [02-auth-users](02-auth-users/) | Auth entity, login, refresh, protected writes |
| [03-ecommerce](03-ecommerce/) | Multiple entities with foreign keys and validators |
| [04-access-rules](04-access-rules/) | public / auth / custom rule modes |
| [05-realtime](05-realtime/) | SSE subscription and live updates |
| [06-files](06-files/) | File upload fields and multipart requests |
| [07-scripting](07-scripting/) | Custom routes via `index.mantis.js` |
| [migrations](migrations/) | Schema migrations and dump/restore |
| [deployment](deployment/) | Production Docker + PostgreSQL |

## Conventions

- **Schemas** are JSON ready for `POST /api/v1/schemas` (admin token required) or `mantisbase migrate apply --up`.
- **HTTP scripts** use placeholders: set `BASE_URL`, `ADMIN_TOKEN`, and `USER_TOKEN` before running.
- **Expected status codes** are noted in each README where non-obvious (401, 403, 404).

## Documentation

- [Quick Start](../doc/QuickStart.md)
- [API Reference](../doc/api.md)
- [Authentication](../doc/auth.md)
- [Access Rules](../doc/rules.md)

For AI-assisted development, see [AGENTS.md](../AGENTS.md) and `.cursor/skills/`.
