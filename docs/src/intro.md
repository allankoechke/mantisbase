# Introduction

MantisBase is a lightweight schema-driven backend. The Rust implementation stores catalog metadata in
`mb_*` system tables, serves `/api/v1/*` over Axum, serves the **compiled** admin UI at `/mb/` from
`public/mb-dist` (Vite build in `admin/`), and defaults to a local **libSQL** database file under the
configured data directory.
