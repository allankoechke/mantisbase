use std::path::PathBuf;
use clap::{Args, Parser, Subcommand, ValueEnum};

// mantisbase --data-dir --scripts-dir --public-dir --dev --db --db-url
// ... serve --port --host
// ... admins --add --ls --rm
// ... migrate
#[derive(Parser, Debug)]
struct Cli {
    /// Data directory path
    #[arg(long)]
    data_dir: Option<PathBuf>,

    /// Scripts directory path
    #[arg(long)]
    scripts_dir: Option<PathBuf>,

    /// Public directory path for static files
    #[arg(long)]
    public_dir: Option<PathBuf>,

    /// Enable development mode (verbose logging)
    #[arg(long)]
    dev: bool,

    /// Database type selection
    #[arg(long)]
    db: Option<DatabaseType>,

    /// Database connection URL
    #[arg(long)]
    db_url: Option<String>,

    /// Optional subcommands
    #[command(subcommand)]
    commands: Option<Commands>,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, ValueEnum)]
enum DatabaseType {
    SQLITE,
    POSTGRESQL,
    MYSQL,
}

#[derive(Subcommand, Debug)]
enum Commands {
    /// Initialize http listening server
    Serve(ServeArgs),
    /// Manage admin users
    Admins(AdminArgs),
    /// Database Migrations
    Migrations(MigrationArgs),
}

#[derive(Args, Debug)]
struct ServeArgs {
    #[arg(long, default_value = "7070",
    value_parser = clap::value_parser!(u16).range(1..),
    help = "Port number to listen on")]
    port: Option<u16>,

    #[arg(long, default_value = "127.0.0.1", help = "Host address to bind to")]
    host: Option<String>,
}

#[derive(Args, Debug)]
struct AdminArgs {

}

#[derive(Args, Debug)]
struct MigrationArgs {

}

struct MantisBase {}
impl MantisBase {
    fn new() -> Self {
        Self {}
    }

    fn set_data_dir(&self, path: PathBuf) {
        println!("Setting data directory to {:?}", path);
    }

    fn set_scripts_dir(&self, path: PathBuf) {
        println!("Setting scripts directory to {:?}", path);
    }

    fn set_public_dir(&self, path: PathBuf) {
        println!("Setting public directory to {:?}", path);
    }
}

fn main() {
    let cli = Cli::parse();

    // Create MantisBase instance
    let mantisbase = MantisBase::new();

    // Set data directory
    if let Some(data_dir) = cli.data_dir {
        println!("Data directory: {:?}", data_dir);
        mantisbase.set_data_dir(data_dir);
    }

    // Set customization scripts directory
    if let Some(scripts_dir) = cli.scripts_dir {
        println!("Scripts directory: {:?}", scripts_dir);
        mantisbase.set_scripts_dir(scripts_dir);
    }

    // Set static files directory
    if let Some(public_dir) = cli.public_dir {
        println!("Public directory: {:?}", public_dir);
        mantisbase.set_public_dir(public_dir);
    }

    // Set development mode
    if cli.dev {
        println!("Development mode enabled");
    }

    // SQLite is the default database to be used
    let db_type = cli.db.unwrap_or(DatabaseType::SQLITE);
    let mut db_url = cli.db_url.unwrap_or_else(String::new); // Use empty string if not provided

    println!("Database type: {:?}", db_type);

    // If db URL is not SQLite, check for connection URL
    if db_type != DatabaseType::SQLITE {
        if db_url.trim().is_empty() {
            // Check if connection URL is provided via env vars, else panic
            if let Ok(url) = std::env::var("MB_DATABASE_URL") {
                println!("Database connection URL: {}", url);
                db_url = url;
            } else {
                panic!("Database connection URL is required for non-SQLite databases");
            }
        } else {
            println!("Database connection URL: {}", db_url);
        }
    }

    // Check on subcommands //
    // ---------------------//
    match &cli.commands {
        Some(Commands::Serve(serve_args)) => {
            println!("Serving on port: {:?}", serve_args.port.unwrap());
        }
        Some(Commands::Admins(_)) => {
            println!("Managing admin users");
        }
        Some(Commands::Migrations(_)) => {
            println!("Running database migrations");
        }
        None => {
            println!("No subcommand provided, exiting");
        }
    }
}
