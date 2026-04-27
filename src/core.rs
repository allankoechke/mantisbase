use crate::config::LogLevel;

#[derive(Debug)]
pub enum MantisBaseDbType {
    Sqlite,
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
    // Base Options
    data_dir: String,
    scripts_dir: String,
    public_dir: String,
    db_url: String,
    db_type: MantisBaseDbType,
    log_level: LogLevel, // default Info
    mode: MantisBaseMode,

    // Serve Options
    port: u16, // default 7070
    host: String, // default 127.0.0.1
}

impl MantisBase {
    pub fn new() -> Self {
        Self {
            data_dir: "./data".to_string(),
            scripts_dir: "./scripts".to_string(),
            public_dir: "./public".to_string(),
            db_url: "".to_string(),
            db_type: MantisBaseDbType::Sqlite,
            log_level: LogLevel::Info,
            mode: MantisBaseMode::None,
            port: 7070,
            host: "127.0.0.1".to_string(),
        }
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

    pub fn db_type(&self) -> &MantisBaseDbType {
        &self.db_type
    }

    pub fn set_db_type(&mut self, db_type: MantisBaseDbType) {
        self.db_type = db_type;
    }

    pub fn log_level(&self) -> &LogLevel {
        &self.log_level
    }

    pub fn set_log_level(&mut self, log_level: LogLevel) {
        self.log_level = log_level;
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
}