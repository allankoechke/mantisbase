# Introduction

MantisBase is a lightweight schema-driven backend. The Rust implementation stores catalog metadata in
`mb_*` system tables, serves `/api/v1/*` over Axum, and defaults to a local **libSQL** database file under
the configured data directory.
