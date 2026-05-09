//! Core runtime configuration for directories, database selection, and HTTP listen options.
use spdlog::warn;

use crate::logger::{default_logger, Level, LevelFilter};

/// Database backend selected at runtime.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MantisBaseDbType {
    /// Local libSQL file (default).
    Libsql,
    /// Turso / remote libSQL (`libsql://` URL).
    Turso,
    /// PostgreSQL (`postgres` Cargo feature; `--db postgresql`).
    #[cfg(feature = "postgres")]
    Postgres,
}

#[derive(Debug)]
pub enum MantisBaseMode {
    None,
    Serve,
    Admin,
    Migrations,
}

#[derive(Debug)]
pub struct MantisBase {
    data_dir: String,
    scripts_dir: String,
    public_dir: String,
    db_url: String,
    db_type: MantisBaseDbType,
    log_level: Level,
    mode: MantisBaseMode,
    port: u16,
    host: String,
    /// Used to sign end-user JWTs (`auth` entities); optional in dev.
    jwt_secret: Option<String>,
}

impl MantisBase {
    pub fn new() -> Self {
        default_logger().set_level_filter(LevelFilter::MoreSevereEqual(Level::Info));

        Self {
            data_dir: "./data".to_string(),
            scripts_dir: "./scripts".to_string(),
            public_dir: "./public".to_string(),
            db_url: String::new(),
            db_type: MantisBaseDbType::Libsql,
            log_level: Level::Info,
            mode: MantisBaseMode::None,
            port: 7070,
            host: "127.0.0.1".to_string(),
            jwt_secret: None,
        }
        .init()
    }

    fn init(mut self) -> Self {
        if let Ok(log_level) = std::env::var("MB_LOG_LEVEL") {
            match log_level.to_lowercase().as_str() {
                "trace" => self.set_log_level(Level::Trace),
                "debug" => self.set_log_level(Level::Debug),
                "info" => self.set_log_level(Level::Info),
                "warn" => self.set_log_level(Level::Warn),
                "error" => self.set_log_level(Level::Error),
                "critical" => self.set_log_level(Level::Critical),
                _ => {
                    warn!("Invalid log level provided '{log_level}', defaulting to Info");
                    self.set_log_level(Level::Info)
                }
            }
        }

        if let Ok(jwt_secret) = std::env::var("MB_JWT_SECRET") {
            self.jwt_secret = Some(jwt_secret);
        } else {
            warn!(
                "MB_JWT_SECRET not set — user JWT issuance disabled for production-grade setups."
            );
        }

        self
    }

    pub fn set_data_dir(&mut self, data_dir: &str) {
        self.data_dir = data_dir.to_string();
    }

    pub fn data_dir(&self) -> &str {
        &self.data_dir
    }

    pub fn set_scripts_dir(&mut self, scripts_dir: &str) {
        self.scripts_dir = scripts_dir.to_string();
    }

    pub fn scripts_dir(&self) -> &str {
        &self.scripts_dir
    }

    pub fn public_dir(&self) -> &str {
        &self.public_dir
    }

    pub fn set_public_dir(&mut self, public_dir: &str) {
        self.public_dir = public_dir.to_string();
    }

    pub fn db_url(&self) -> &str {
        &self.db_url
    }

    pub fn set_db_url(&mut self, db_url: &str) {
        self.db_url = db_url.to_string();
    }

    pub fn db_type(&self) -> MantisBaseDbType {
        self.db_type
    }

    pub fn set_db_type(&mut self, db_type: MantisBaseDbType) {
        self.db_type = db_type;
    }

    pub fn log_level(&self) -> &Level {
        &self.log_level
    }

    pub fn set_log_level(&mut self, log_level: Level) {
        self.log_level = log_level;
        default_logger().set_level_filter(LevelFilter::MoreSevereEqual(self.log_level));
    }

    pub fn mode(&self) -> &MantisBaseMode {
        &self.mode
    }

    pub fn set_mode(&mut self, mode: MantisBaseMode) {
        self.mode = mode;
    }

    pub fn port(&self) -> u16 {
        self.port
    }

    pub fn set_port(&mut self, port: u16) {
        self.port = port;
    }

    pub fn host(&self) -> &str {
        &self.host
    }

    pub fn set_host(&mut self, host: &str) {
        self.host = host.to_string();
    }

    pub fn jwt_secret(&self) -> Option<&str> {
        self.jwt_secret.as_deref()
    }
}

impl Default for MantisBase {
    fn default() -> Self {
        Self::new()
    }
}
