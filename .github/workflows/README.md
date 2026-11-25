# GitHub Actions workflows

## Workflows

- **ci.yml** — Build and test on push/PR to `master` and `v0.3.x`. Uses `build-matrix.yml`.
- **release.yml** — On tag push `v*`: build, create GitHub Release with assets, then build and push Docker image. Uses `build-matrix.yml` and `docker-publish.yml`. Stable tags (e.g. `v1.2.0`) create a normal GitHub Release and push Docker tags `{version}` and `latest`. Pre-release tags (e.g. `v1.2.0-beta.1`, `v1.2.0-alpha.1`, `v1.2.0-rc.1`) create a GitHub pre-release and push Docker tags `{version}` plus a moving channel tag (`alpha`, `beta`, or `rc`); they do not update `latest`.
- **docs.yml** — Build Doxygen docs and publish to `gh-pages`. Triggered by tag push `v*` or manually via **Run workflow** (`workflow_dispatch`).
- **build-matrix.yml** — Reusable: matrix build (Linux x86-64 on `ubuntu-latest`, Linux aarch64 on `ubuntu-24.04-arm`, Windows x86-64, Windows ARM64 on `windows-11-arm` via llvm-mingw, macOS aarch64 + x86-64), test, and on tag zip/upload artifacts. Job names show the platform (e.g. `Build (linux-aarch64)`). Library and per-platform header artifacts (`mantisbase_<tag>_<platform>-lib/-headers`) feed both the dev-package and the Python wheels. Platform binary zips include `README.md` and `LICENSE`. macOS dylibs are staged for wheels only — the dev-package supports linux + windows.
- **docker-publish.yml** — Reusable: build image from `docker/`, push to Docker Hub. Stable releases get `{version}` and `latest`; pre-releases get `{version}` and the channel tag (`alpha`, `beta`, or `rc`).
- **bindings-node.yml** — On tag push `v*` or manual dispatch: build the Node.js N-API addon in `bindings/node` across OS × Node LTS, upload prebuilt `.node` binaries as Release assets, then `npm publish`.
- **bindings-python.yml** — On tag push `v*` or manual dispatch: first runs the main build (`engine` job via `build-matrix.yml`), then builds Python wheels per platform (Linux x86-64/aarch64, Windows x86-64/ARM64, macOS aarch64/x86-64) × CPython 3.10–3.14 (Windows ARM64 starts at 3.11; 3.9 is not published). Each wheel job `cmake --install`s nothing itself — it downloads the engine `-lib`/`-headers` artifacts, assembles a standard prefix, runs `pip wheel` against it, repairs the wheel (`auditwheel`/`delocate`/`delvewheel`, which vendors the engine library and stamps the platform tag), installs it, and runs `bindings/python/tests` (schema CRUD, SQL, auth, files, mail, and HTTP routes, not only `import mantisbase`). It then uploads the wheel. Then publishes wheels + sdist to PyPI via trusted publishing. Wheel jobs only run on tags (engine artifacts exist only there). The package version is the CMake `project(MantisBase VERSION ...)` value. Pre-release tags are published as PEP 440 versions: `v1.2.0-alpha.1` → `1.2.0a1`, `v1.2.0-beta.1` → `1.2.0b1`, `v1.2.0-rc.1` → `1.2.0rc1`. A tag whose numeric version differs from CMake fails the job.

## Release tag convention

Use SemVer-style tags:

- Stable: `v1.2.0` → Docker `1.2.0`, `latest`; Python `1.2.0`
- Beta: `v1.2.0-beta.1` → Docker `1.2.0-beta.1`, `beta`; Python `1.2.0b1`
- Alpha: `v1.2.0-alpha.1` → Docker `1.2.0-alpha.1`, `alpha`; Python `1.2.0a1`
- RC: `v1.2.0-rc.1` → Docker `1.2.0-rc.1`, `rc`; Python `1.2.0rc1`