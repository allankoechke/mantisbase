//! Core runtime configuration for directories, database selection, and HTTP listen options.

use crate::cli::{Cli, Commands, DatabaseType};
use crate::logger::prelude::*;
#[cfg(feature = "postgres")]
use crate::storage::PostgresStore;
use crate::storage::{LibsqlStore, Store};

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

/// Result of [`MantisBase::run`]: distinguish “nothing to do” from a completed action.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MantisBaseRunOutcome {
    /// A subcommand ran to completion (including printing help-like migration hint).
    RanAction,
    /// No subcommand was given; see stderr hints from [`MantisBase::run`].
    NoSubcommand,
}

#[derive(Debug)]
pub struct MantisBase {
    data_dir: String,
    scripts_dir: String,
    /// Root of the Vite build output (default `./public/mb-dist`).
    admin_ui_dir: String,
    db_url: String,
    db_type: MantisBaseDbType,
    log_level: Level,
    mode: MantisBaseMode,
    port: u16,
    host: String,
    /// Used to sign application-user JWTs from `POST /api/v1/auth/login`; optional in dev.
    jwt_secret: Option<String>,
    /// Last CLI subcommand (set by [`Self::apply_cli`]).
    cli_command: Option<Commands>,
    /// Database backend from CLI (`--db`), used when opening [`Store`].
    db_backend: DatabaseType,
}

impl MantisBase {
    pub fn new() -> Self {
        default_logger().set_level_filter(LevelFilter::MoreSevereEqual(Level::Info));

        Self {
            data_dir: "./data".to_string(),
            scripts_dir: "./scripts".to_string(),
            admin_ui_dir: "./public/mb-dist".to_string(),
            db_url: String::new(),
            db_type: MantisBaseDbType::Libsql,
            log_level: Level::Info,
            mode: MantisBaseMode::None,
            port: 7070,
            host: "127.0.0.1".to_string(),
            jwt_secret: None,
            cli_command: None,
            db_backend: DatabaseType::Libsql,
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
                "MB_JWT_SECRET not set — default user JWT issuance disabled for production-grade setups."
            );
        }

        if let Ok(dir) = std::env::var("MB_ADMIN_UI_DIR") {
            let t = dir.trim();
            if !t.is_empty() {
                self.admin_ui_dir = t.to_string();
            }
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

    pub fn admin_ui_dir(&self) -> &str {
        &self.admin_ui_dir
    }

    pub fn set_admin_ui_dir(&mut self, dir: &str) {
        self.admin_ui_dir = dir.to_string();
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

    /// Applies global options and subcommand from `cli` (paths, `--dev`, database, listen address).
    /// Call [`Self::run`] after this to execute the selected subcommand.
    ///
    /// Alias: [`Self::parse_cli`].
    pub fn apply_cli(&mut self, cli: &Cli) -> anyhow::Result<()> {
        if let Some(data_dir) = &cli.data_dir {
            trace!("Data directory: {:?}", data_dir);
            self.set_data_dir(data_dir.to_str().unwrap_or("./data"));
        }
        if let Some(scripts_dir) = &cli.scripts_dir {
            trace!("Scripts directory: {:?}", scripts_dir);
            self.set_scripts_dir(scripts_dir.to_str().unwrap_or("./scripts"));
        }
        if let Some(admin_ui) = &cli.admin_ui_dir {
            trace!("Admin UI directory: {:?}", admin_ui);
            self.set_admin_ui_dir(admin_ui.to_str().unwrap_or("./public/mb-dist"));
        }
        if cli.dev {
            self.set_log_level(Level::Trace);
        }

        let db_backend = cli.db.unwrap_or_default();
        self.db_backend = db_backend;
        let mut db_url = cli.db_url.clone().unwrap_or_default();

        let db_type = match db_backend {
            DatabaseType::Libsql => MantisBaseDbType::Libsql,
            DatabaseType::Turso => MantisBaseDbType::Turso,
            #[cfg(feature = "postgres")]
            DatabaseType::Postgresql => MantisBaseDbType::Postgres,
        };
        self.set_db_type(db_type);

        if db_url.trim().is_empty() {
            if let Ok(url) = std::env::var("MB_DATABASE_URL") {
                db_url = url;
            }
        }

        if db_backend != DatabaseType::Libsql && db_url.trim().is_empty() {
            anyhow::bail!(
                "database URL required for Turso/Postgres (use --db-url or MB_DATABASE_URL)"
            );
        }

        self.set_db_url(&db_url);

        if let Some(Commands::Serve(serve_args)) = &cli.commands {
            if let Some(p) = serve_args.port {
                self.set_port(p);
            }
            if let Some(h) = &serve_args.host {
                self.set_host(h);
            }
        }

        self.cli_command = cli.commands.clone();
        self.set_mode(match &cli.commands {
            None => MantisBaseMode::None,
            Some(Commands::Serve(_)) => MantisBaseMode::Serve,
            Some(Commands::Admins(_)) => MantisBaseMode::Admin,
            Some(Commands::Migrations(_)) => MantisBaseMode::Migrations,
        });

        Ok(())
    }

    /// Same as [`Self::apply_cli`]; name matches “parse then run” embedding flows.
    pub fn parse_cli(&mut self, cli: &Cli) -> anyhow::Result<()> {
        self.apply_cli(cli)
    }

    /// Builds a new instance and applies `cli` in one step.
    pub fn from_cli(cli: &Cli) -> anyhow::Result<Self> {
        let mut m = Self::new();
        m.apply_cli(cli)?;
        Ok(m)
    }

    /// Database backend selected on the CLI (`--db`).
    pub fn db_backend(&self) -> DatabaseType {
        self.db_backend
    }

    /// Parsed subcommand, if any (after [`Self::apply_cli`]).
    pub fn cli_command(&self) -> Option<&Commands> {
        self.cli_command.as_ref()
    }

    /// Runs the action implied by the last [`Self::apply_cli`] (or [`Self::from_cli`]).
    pub async fn run(&self) -> anyhow::Result<MantisBaseRunOutcome> {
        match self.mode() {
            MantisBaseMode::Serve => {
                let store = match self.db_backend {
                    DatabaseType::Libsql => Store::Libsql(
                        LibsqlStore::open_local(self.data_dir(), self.db_url()).await?,
                    ),
                    DatabaseType::Turso => {
                        let token = std::env::var("MB_TURSO_AUTH_TOKEN").map_err(|_| {
                            anyhow::anyhow!("MB_TURSO_AUTH_TOKEN required for Turso")
                        })?;
                        Store::Libsql(LibsqlStore::open_remote(self.db_url(), &token).await?)
                    }
                    #[cfg(feature = "postgres")]
                    DatabaseType::Postgresql => {
                        Store::Postgres(PostgresStore::connect(self.db_url()).await?)
                    }
                };
                crate::http::serve(self, store).await?;
                Ok(MantisBaseRunOutcome::RanAction)
            }
            MantisBaseMode::Admin => {
                let Some(Commands::Admins(admin_args)) = &self.cli_command else {
                    anyhow::bail!("internal error: admin mode without admins subcommand");
                };
                let store = match self.db_backend {
                    DatabaseType::Libsql | DatabaseType::Turso => Store::Libsql(
                        LibsqlStore::open_local(self.data_dir(), self.db_url()).await?,
                    ),
                    #[cfg(feature = "postgres")]
                    DatabaseType::Postgresql => {
                        Store::Postgres(PostgresStore::connect(self.db_url()).await?)
                    }
                };
                if let Some(add_args) = &admin_args.add {
                    if add_args.len() != 2 {
                        anyhow::bail!("admins add requires EMAIL PASSWORD");
                    }
                    store.add_admin(&add_args[0], &add_args[1]).await?;
                    info!("admin created");
                } else if admin_args.ls {
                    for (id, email) in store.list_admins().await? {
                        crate::logger::cli_stdout_line(format!("{id}\t{email}"));
                    }
                } else if let Some(target) = &admin_args.rm {
                    let n = store.remove_admin(target).await?;
                    info!("removed {n} admin row(s)");
                }
                Ok(MantisBaseRunOutcome::RanAction)
            }
            MantisBaseMode::Migrations => {
                let Some(Commands::Migrations(migration_args)) = &self.cli_command else {
                    anyhow::bail!("internal error: migrations mode without migrations subcommand");
                };
                let store = match self.db_backend {
                    DatabaseType::Libsql | DatabaseType::Turso => Store::Libsql(
                        LibsqlStore::open_local(self.data_dir(), self.db_url()).await?,
                    ),
                    #[cfg(feature = "postgres")]
                    DatabaseType::Postgresql => {
                        Store::Postgres(PostgresStore::connect(self.db_url()).await?)
                    }
                };
                if migration_args.up {
                    store.migrate().await?;
                    info!("migrations applied");
                } else if migration_args.reset {
                    anyhow::bail!(
                        "destructive reset not implemented; remove data/mantis.db manually"
                    );
                } else {
                    info!("use --up to apply migrations");
                }
                Ok(MantisBaseRunOutcome::RanAction)
            }
            MantisBaseMode::None => {
                print_no_subcommand_hint();
                Ok(MantisBaseRunOutcome::NoSubcommand)
            }
        }
    }
}

fn print_no_subcommand_hint() {
    warn!(
        "No subcommand given.\n\n\
        Examples:\n\
          mantisbase serve                      HTTP API + static /mb\n\
          mantisbase admins --add EMAIL PASS    create an admin (or use POST /api/v1/admins)\n\
          mantisbase migrations --up            apply catalog migrations\n\n\
        For all options:  mantisbase --help\n\
        Database:         --db libsql|turso|postgresql   --db-url …   (or MB_DATABASE_URL)"
    );
}

impl Default for MantisBase {
    fn default() -> Self {
        Self::new()
    }
}
