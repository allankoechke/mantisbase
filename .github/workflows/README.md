# GitHub Actions workflows

## Workflows

- **ci.yml** — Build and test on push/PR to `master` and `v0.3.x`. Uses `build-matrix.yml`.
- **release.yml** — On tag push `v*`: build, create GitHub Release with assets, then build and push Docker image. Uses `build-matrix.yml` and `docker-publish.yml`. Stable tags (e.g. `v1.2.0`) create a normal GitHub Release and push Docker tags `{version}` and `latest`. Pre-release tags (e.g. `v1.2.0-beta.1`, `v1.2.0-alpha.1`, `v1.2.0-rc.1`) create a GitHub pre-release and push Docker tags `{version}` plus a moving channel tag (`alpha`, `beta`, or `rc`); they do not update `latest`.
- **docs.yml** — Build Doxygen docs and publish to `gh-pages`. Triggered by tag push `v*` or manually via **Run workflow** (`workflow_dispatch`).
- **build-matrix.yml** — Reusable: matrix build (Linux x86-64, Linux aarch64, Windows x86-64), test, and on tag zip/upload artifacts. Job names show the platform (e.g. `Build (linux-aarch64)`). Library artifacts use `lib/<platform>/<mode>/<architecture>/` (e.g. `lib/linux/shared/x86-64/libmantisbase.so`). Platform binary zips include `README.md` and `LICENSE`.
- **docker-publish.yml** — Reusable: build image from `docker/`, push to Docker Hub. Stable releases get `{version}` and `latest`; pre-releases get `{version}` and the channel tag (`alpha`, `beta`, or `rc`).

## Release tag convention

Use SemVer-style tags:

- Stable: `v1.2.0` → Docker `1.2.0`, `latest`
- Beta: `v1.2.0-beta.1` → Docker `1.2.0-beta.1`, `beta`
- Alpha: `v1.2.0-alpha.1` → Docker `1.2.0-alpha.1`, `alpha`
- RC: `v1.2.0-rc.1` → Docker `1.2.0-rc.1`, `rc`