# Contributing to MantisBase

Thank you for contributing to MantisBase — a self-hosted C++20 Backend-as-a-Service with auto-generated REST APIs, authentication, realtime SSE, file uploads, and an admin dashboard.

## Development setup

### Prerequisites

- C++20 compiler (GCC or MinGW 13+)
- CMake 3.22 or higher
- Git with submodule support
- Linux only: `libpq-dev` and `uuid-dev` (PostgreSQL support)

### Build

```bash
git clone --recurse-submodules https://github.com/allankoechke/mantisbase.git
cd mantisbase
cmake -B build
cmake --build build
./build/bin/mantisbase serve
```

### Tests

Tests are disabled by default. Enable them at configure time:

```bash
cmake -B build -DMB_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

CI runs the same test suite on Linux for every pull request.

### Useful CMake options

| Option | Default | Description |
|---|---|---|
| `MB_BUILD_TESTS` | OFF | Build and register unit/integration tests |
| `MB_SHARED_DEPS` | OFF | Build dependencies as shared libraries |
| `MB_BUILD_SHARED_LIB` | OFF | Also produce a shared `libmantisbase` alongside the static library |
| `MB_SCRIPTING_ENABLED` | OFF | Enable JavaScript extensions via Duktape |
| `MB_BUILD_DOCS` | OFF | Generate Doxygen documentation |
| `MB_BUILD_WITH_ASAN` | ON | AddressSanitizer on Linux debug builds |

## Project layout

```
mantisbase/
├── include/mantisbase/   # Public headers
├── src/                  # Core implementation
├── tests/                # Unit and integration tests
├── doc/                  # User and API documentation
├── cmake/                # Dependency and build modules
├── 3rdParty/             # Git submodules (Drogon, SOCI, etc.)
├── libs/                 # Additional bundled libraries
└── docker/               # Container deployment
```

Key dependencies are managed as git submodules under `3rdParty/` and wired in through `cmake/`.

## Guidelines

### Code style

- Use C++20 and match existing patterns in the surrounding code.
- Keep changes focused; avoid unrelated refactors in the same pull request.

### Commit messages

Use clear, descriptive messages. Conventional prefixes are welcome:

```
feat: add API key rotation endpoint
fix: resolve SSE reconnect race on PostgreSQL
docs: update auth guide for OAuth providers
test: cover expired JWT refresh flow
```

### Pull requests

1. Fork the repository and create a branch from `master`.
2. Make your changes and add tests when behavior changes.
3. Ensure the project builds and tests pass (`-DMB_BUILD_TESTS=ON`).
4. Open a pull request with a short summary and links to related issues.

When changing the HTTP API, update `doc/openapi.yaml` and the relevant guides under `doc/`.

## Reporting issues

Search [existing issues](https://github.com/allankoechke/mantisbase/issues) first. Include:

- OS, compiler, and MantisBase version
- Steps to reproduce
- Expected vs actual behavior
- Relevant logs (omit secrets and tokens)

## Getting help

- [Discord](https://discord.gg/9437XTKRvN) — chat and community discussion
- [GitHub Discussions](https://github.com/allankoechke/mantisbase/discussions) — questions and ideas
- [GitHub Issues](https://github.com/allankoechke/mantisbase/issues) — bugs and feature requests
- [Documentation](https://allankoechke.github.io/mantisbase/) — guides and API reference

## License

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE).
