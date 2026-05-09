//! MantisBase — lightweight schema-driven BaaS (Rust port).
//!
//! ## Overview
//! - **Storage:** [`storage::Store`] ([`storage::LibsqlStore`] by default; optional PostgreSQL with the `postgres` feature).
//! - **HTTP:** [`http::serve`] exposes `/api/v1/*` with **HTTP Basic** admin auth and OpenAPI at `/api/v1/openapi.json`.
//! - **Files:** [`files::LocalFs`] stores blobs under `data_dir/files/…`.
//!
//! ## Example (CLI)
//! Run `mantisbase serve` after creating an admin with `mantisbase admins add …`.

pub mod auth;
pub mod cli;
pub mod config;
pub mod core;
pub mod db;
pub mod files;
pub mod http;
pub mod logger;
pub mod mail;
pub mod models;
pub mod realtime;
pub mod scripts;
pub mod storage;
pub mod util_time;
pub mod webhooks;

pub use core::{MantisBase, MantisBaseDbType, MantisBaseMode};
