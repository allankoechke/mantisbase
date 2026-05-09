use std::path::PathBuf;

use clap::{ArgGroup, Args, Parser, Subcommand, ValueEnum};

#[derive(Parser, Debug)]
#[command(name = "mantisbase", version, about)]
pub struct Cli {
    /// Data directory path
    #[arg(long)]
    pub data_dir: Option<PathBuf>,

    /// Scripts directory path
    #[arg(long)]
    pub scripts_dir: Option<PathBuf>,

    /// Public directory path for static files
    #[arg(long)]
    pub public_dir: Option<PathBuf>,

    /// Enable development mode (verbose logging)
    #[arg(long)]
    pub dev: bool,

    /// Database backend (default: local libSQL file under data dir)
    #[arg(long)]
    pub db: Option<DatabaseType>,

    /// Database connection URL (Turso token URL or Postgres connection string)
    #[arg(long)]
    pub db_url: Option<String>,

    #[command(subcommand)]
    pub commands: Option<Commands>,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, ValueEnum)]
pub enum DatabaseType {
    /// Local libSQL database file (default).
    Libsql,
    /// Remote Turso / libSQL (`libsql://`…).
    Turso,
    /// PostgreSQL (`MB_DATABASE_URL` or `--db-url`).
    #[cfg(feature = "postgres")]
    Postgresql,
}

#[derive(Subcommand, Debug)]
pub enum Commands {
    /// Run HTTP server
    Serve(ServeArgs),
    /// Manage admin users
    Admins(AdminArgs),
    /// Database migrations
    Migrations(MigrationArgs),
}

#[derive(Args, Debug)]
pub struct ServeArgs {
    #[arg(
        long,
        default_value = "7070",
        value_parser = clap::value_parser!(u16).range(1..),
        help = "Port number to listen on"
    )]
    pub port: Option<u16>,

    #[arg(long, default_value = "127.0.0.1", help = "Host address to bind to")]
    pub host: Option<String>,
}

#[derive(Args, Debug)]
#[command(group(
    ArgGroup::new("admin_action")
        .args(["add", "ls", "rm"])
        .required(true)
        .multiple(false)
))]
pub struct AdminArgs {
    #[arg(
        long,
        short = 'a',
        num_args = 2,
        value_names = ["EMAIL", "PASSWORD"],
        help = "Add an admin user"
    )]
    pub add: Option<Vec<String>>,

    #[arg(long, short = 'l', help = "List all admin users")]
    pub ls: bool,

    #[arg(long, short = 'r', help = "Remove an admin user by email or id")]
    pub rm: Option<String>,
}

#[derive(Args, Debug)]
#[command(group(
    ArgGroup::new("migration_action")
        .args(["up", "down", "reset"])
        .required(true)
        .multiple(false)
))]
pub struct MigrationArgs {
    #[arg(long, short = 'u', help = "Apply all pending migrations")]
    pub up: bool,

    #[arg(long, short = 'd', help = "Reserved: downgrade migrations")]
    pub down: Option<u32>,

    #[arg(long, short = 'r', help = "Reset database schema (destructive)")]
    pub reset: bool,
}
