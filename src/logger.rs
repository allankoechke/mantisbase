//! Logging facade for MantisBase.
//!
//! Use the macros from [`prelude`] (`info!`, `debug!`, `trace!`, `warn!`, `error!`, `critical!`)
//! for all **diagnostic** output. Do not use the `tracing` crate, raw `spdlog` imports, or
//! `println!` / `eprintln!` for logs elsewhere in this crate.
//!
//! Backed by [spdlog-rs](https://docs.rs/spdlog-rs). [`MantisBase::new`](crate::core::MantisBase::new)
//! configures the default logger level; override with `MB_LOG_LEVEL`.
//!
//! ## Stdout for scripts
//! Tab-separated lines for `admins --ls` are written with [`cli_stdout_line`] so they stay
//! clean for piping; that is **program output**, not a log line.

pub use spdlog::prelude::*;
pub use spdlog::{default_logger, Level, LevelFilter};

/// Common imports: `use mantisbase::logger::prelude::*;` or `use crate::logger::prelude::*;`
pub mod prelude {
    pub use spdlog::prelude::*;
    pub use spdlog::{default_logger, Level, LevelFilter};
}

/// One line of machine-oriented CLI output to **stdout** (e.g. `admins --ls`), without log formatting.
#[inline]
pub fn cli_stdout_line(line: impl AsRef<str>) {
    use std::io::Write;
    let _ = writeln!(std::io::stdout(), "{}", line.as_ref());
}
