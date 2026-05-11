//! MantisBase — lightweight schema-driven BaaS (Rust port).
//!
//! ## Overview
//! - **Storage:** [`storage::Store`] ([`storage::LibsqlStore`] by default; optional PostgreSQL with the `postgres` feature).
//! - **HTTP:** [`http::serve`] exposes `/api/v1/*` with **HTTP Basic** admin auth and OpenAPI at `/api/v1/openapi.json`.
//! - **Logging:** [`logger`] (`info!`, `debug!`, …); do not use `tracing` or `println!` for diagnostics in crate code.
//! - **Files:** [`files::LocalFs`] stores blobs under `data_dir/files/…`.
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
pub mod webhooks;

pub use core::{MantisBase, MantisBaseDbType, MantisBaseMode, MantisBaseRunOutcome};
