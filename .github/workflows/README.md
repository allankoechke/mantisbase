# GitHub Actions workflows

## Workflows

- **ci.yml** — Build and test on push/PR to `master` and `v0.3.x`. Uses `build-matrix.yml`.
- **release.yml** — On tag push `v*`: build, create GitHub Release with assets, then build and push Docker image. Uses `build-matrix.yml` and `docker-publish.yml`.
- **docs.yml** — Build Doxygen docs and publish to `gh-pages`. Triggered by tag push `v*` or manually via **Run workflow** (`workflow_dispatch`).
- **build-matrix.yml** — Reusable: matrix build (Ubuntu/Windows), test, and on tag zip/upload artifacts.
- **docker-publish.yml** — Reusable: build image from `docker/`, push to Docker Hub with version and `latest` tags.