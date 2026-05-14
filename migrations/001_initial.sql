-- MantisBase system catalog (libSQL / SQLite). Embedded at boot; change only with breaking catalog updates.
-- Postgres equivalent: `migrations/postgres/20240509120000_initial.sql`.

CREATE TABLE IF NOT EXISTS mb_migration (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS mb_admin (
    id TEXT PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- Application users (single catalog; passwords managed here, not per-entity tables)
CREATE TABLE IF NOT EXISTS mb_user (
    id TEXT PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    profile_json TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- One row per entity: JSON document holds type, flags, fields[], rules{}, and optional `extra`.
CREATE TABLE IF NOT EXISTS mb_entity_schema (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    document TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS mb_app_config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    updated_by TEXT,
    FOREIGN KEY (updated_by) REFERENCES mb_admin(id)
);

CREATE TABLE IF NOT EXISTS mb_file (
    id TEXT PRIMARY KEY,
    entity_name TEXT NOT NULL,
    record_id TEXT NOT NULL,
    logical_name TEXT NOT NULL,
    mime TEXT NOT NULL,
    size INTEGER NOT NULL,
    checksum TEXT NOT NULL,
    storage_key TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS mb_webhook (
    id TEXT PRIMARY KEY,
    url TEXT NOT NULL,
    secret_hmac TEXT,
    events TEXT NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL
);

-- Ledger for optional *.sql in the configured migrations directory (see --migrations-dir).
CREATE TABLE IF NOT EXISTS mb_sql_dir_migration (
    relpath TEXT PRIMARY KEY,
    applied_at TEXT NOT NULL
);

INSERT OR IGNORE INTO mb_migration (version, applied_at) VALUES (1, datetime('now'));
