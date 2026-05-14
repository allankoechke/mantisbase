//! MantisBase — lightweight schema-driven BaaS (Rust port).
//!
//! ## Overview
//! - **Storage:** [`storage::Store`] ([`storage::LibsqlStore`] or [`storage::PostgresStore`]; backend chosen with `--db` at runtime).
//! - **HTTP:** [`http::serve`] exposes `/api/v1/*` (admin **HTTP Basic** on `/api/v1/sys/*` and `/api/v1/admins/*`), OpenAPI at `/api/v1/openapi.json`, and the built admin SPA at `/mb/`.
//! - **Logging:** [`logger`] (`info!`, `debug!`, …); do not use `tracing` or `println!` for diagnostics in crate code.
//! - **Files:** [`files::LocalFs`] stores blobs under `data_dir/files/…`.
//! - **Paths:** After [`core::MantisBase::apply_cli`], default-style relative `data`, `migrations`, `scripts`, and admin UI paths are resolved next to the `mantisbase` executable and those directories are created if missing.
//!
//! ## Example (CLI binary)
//! Run `mantisbase serve` after creating an admin with `mantisbase admins --add …`.
//!
//! ## Example (library, same behavior as the binary)
//! ```no_run
//! use clap::Parser;
//! use mantisbase::cli::Cli;
//! use mantisbase::core::MantisBase;
//!
//! # async fn demo() -> anyhow::Result<()> {
//! let cli = Cli::parse_from(["mantisbase", "serve", "--port", "7070"]);
//! let mut mantis = MantisBase::new();
//! mantis.apply_cli(&cli)?; // or: mantis.parse_cli(&cli)?
//! let _ = mantis.run().await?;
//! # Ok(())
//! # }
//! ```

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
pub(crate) mod util_paths;
pub mod webhooks;

pub use core::{MantisBase, MantisBaseDbType, MantisBaseMode, MantisBaseRunOutcome};
