use std::path::PathBuf;
use clap::{Parser, Subcommand, ValueEnum};

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
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, ValueEnum)]
enum DatabaseType {
    SQLITE,
    POSTGRESQL,
    MYSQL,
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

    if let Some(data_dir) = cli.data_dir {
        println!("Data directory: {:?}", data_dir);
        mantisbase.set_data_dir(data_dir);
    }

    if cli.dev {
        println!("Development mode enabled");
    }

    if let Some(db) = cli.db {
        println!("Database type: {:?}", db);
    }

    // If db URL is not SQLite, check for connection URL
    if let Some(db_url) = cli.db_url {
        if cli.db != Some(DatabaseType::SQLITE) {
            println!("Database connection URL: {:?}", db_url);
        }
    } else {
        // Panic that no connection URL was provided through env variables
    }
}
