# ADR 001: Realtime delivery (deferred)

## Context

MantisBase targets embedded and scaled deployments with multiple database backends (local libSQL, Turso,
PostgreSQL).

## Decision (planned)

- Introduce a `RealtimeSource` abstraction per storage backend.
- Evaluate **SSE** vs **WebSockets** (or both, configurable via `mb_app_config`) after core APIs stabilize.

## Status

Deferred — see project plan milestone for realtime.
