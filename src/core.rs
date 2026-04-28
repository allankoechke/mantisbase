//!
use spdlog::warn;
use crate::config::LogLevel;
use crate::logger::{default_logger, LevelFilter, Level};

///
#[derive(Debug)]
pub enum MantisBaseDbType {
    Sqlite,
    Postgres,
}

///
#[derive(Debug)]
pub enum MantisBaseMode {
    None,
    Serve,
    Admin,
    Migrations,
}

///
#[derive(Debug)]
pub struct MantisBase {
    // Base Options
    data_dir: String,
    scripts_dir: String,
    public_dir: String,
    db_url: String,
    db_type: MantisBaseDbType,
    log_level: Level, // default Info
    mode: MantisBaseMode,

    // Serve Options
    port: u16, // default 7070
    host: String, // default 127.0.0.1

    // Internal flags
    jwt_secret: Option<String>,
}

impl MantisBase {
    ///
    pub fn new() -> Self {
        default_logger()
            .set_level_filter(LevelFilter::MoreSevereEqual(Level::Info));

        Self {
            data_dir: "./data".to_string(),
            scripts_dir: "./scripts".to_string(),
            public_dir: "./public".to_string(),
            db_url: "".to_string(),
            db_type: MantisBaseDbType::Sqlite,
            log_level: Level::Info,
            mode: MantisBaseMode::None,
            port: 7070,
            host: "127.0.0.1".to_string(),
            jwt_secret: None,
        }.init() //Run init to setup env flags
    }

    ///
    fn init(mut self) -> Self {
        // Check if log level was set via env vars
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
                }, // Ignore invalid log levels
            }
        }

        if let Ok(jwt_secret) = std::env::var("MB_JWT_SECRET") {
            self.jwt_secret = Some(jwt_secret);
        } else {
            warn!("JWT secret not provided!\
            \n\t-----------------------------------------------\
            \n\tDefaulting to None, this is not recommended for production!\
            \n\tPlease set MB_JWT_SECRET to a secure value\
            \n\tExample: `MB_JWT_SECRET=supersecret`\
            \n\tThis warning may cause a panic in the future if not set!\
            \n\t-----------------------------------------------");
        }

        self
    }

    ///
    pub fn set_data_dir(&mut self, data_dir: &str) {
        self.data_dir = data_dir.to_string();
    }

    ///
    pub fn data_dir(&self) -> &str {
        &self.data_dir
    }

    ///
    pub fn set_scripts_dir(&mut self, scripts_dir: &str) {
        self.scripts_dir = scripts_dir.to_string();
    }

    ///
    pub fn scripts_dir(&self) -> &str {
        &self.scripts_dir
    }

    ///
    pub fn public_dir(&self) -> &str {
        &self.public_dir
    }

    ///
    pub fn set_public_dir(&mut self, public_dir: &str) {
        self.public_dir = public_dir.to_string();
    }

    ///
    pub fn db_url(&self) -> &str {
        &self.db_url
    }

    ///
    pub fn set_db_url(&mut self, db_url: &str) {
        self.db_url = db_url.to_string();
    }

    ///
    pub fn db_type(&self) -> &MantisBaseDbType {
        &self.db_type
    }

    ///
    pub fn set_db_type(&mut self, db_type: MantisBaseDbType) {
        self.db_type = db_type;
    }

    ///
    pub fn log_level(&self) -> &Level {
        &self.log_level
    }

    ///
    pub fn set_log_level(&mut self, log_level: Level) {
        self.log_level = log_level;
        default_logger()
            .set_level_filter(LevelFilter::MoreSevereEqual(self.log_level));
    }

    ///
    pub fn mode(&self) -> &MantisBaseMode {
        &self.mode
    }

    ///
    pub fn set_mode(&mut self, mode: MantisBaseMode) {
        self.mode = mode;
    }

    ///
    pub fn port(&self) -> u16 {
        self.port
    }

    ///
    pub fn set_port(&mut self, port: u16) {
        self.port = port;
    }

    ///
    pub fn host(&self) -> &str {
        &self.host
    }

    ///
    pub fn set_host(&mut self, host: &str) {
        self.host = host.to_string();
    }
}