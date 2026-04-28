use std::path::PathBuf;
use clap::{Args, Parser, Subcommand, ValueEnum, ArgGroup};

use mantisbase::cli::{Cli, Commands, DatabaseType};
use mantisbase::core::MantisBase;
use logger::{Level};

fn main() {
    let cli = Cli::parse();

    // Create MantisBase instance
    let mut mantisbase = MantisBase::new();

    // Set data directory
    if let Some(data_dir) = cli.data_dir {
        println!("Data directory: {:?}", data_dir);
        mantisbase.set_data_dir(data_dir.to_str().unwrap());
    }

    // Set customization scripts directory
    if let Some(scripts_dir) = cli.scripts_dir {
        println!("Scripts directory: {:?}", scripts_dir);
        mantisbase.set_scripts_dir(scripts_dir.to_str().unwrap());
    }

    // Set static files directory
    if let Some(public_dir) = cli.public_dir {
        println!("Public directory: {:?}", public_dir);
        mantisbase.set_public_dir(public_dir.to_str().unwrap());
    }

    // Set development mode
    if cli.dev {
        println!("Development mode enabled");

    } else {
        // Check if log level was set via env vars
        if let Ok(log_level) = std::env::var("MB_LOG_LEVEL") {
            match log_level.to_lowercase().as_str() {
                "trace" => mantisbase.set_log_level(Level::Trace),
                "debug" => mantisbase.set_log_level(Level::Debug),
                "info" => mantisbase.set_log_level(Level::Info),
                "warn" => mantisbase.set_log_level(Level::Warn),
                "error" => mantisbase.set_log_level(Level::Error),
                "critical" => mantisbase.set_log_level(Level::Critical),
                _ => mantisbase.set_log_level(Level::Info), // Ignore invalid log levels
            }
        }
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
        Some(Commands::Admins(admin_args)) => {
            println!("Managing admin users");
            if let Some(add_args) = &admin_args.add {
                // Ensure the args are two in the vec
                if add_args.len() != 2 {
                    panic!("Invalid number of arguments for admin user add");
                }

                let (email, password) = (add_args[0].clone(), add_args[1].clone());
                println!("Adding admin user: {} with password: {}", email, password);
            }
                // List all admins
            else if admin_args.ls {
                println!("Listing all admin users");
            }

            else if let Some(admin_id_or_email) = &admin_args.rm {
                println!("Removing admin user: {}", admin_id_or_email);
            }

            else {
                println!("Invalid admin command provided");
                // panic here?
            }
        }
        Some(Commands::Migrations(migration_args)) => {
            println!("Running database migrations");
        }
        None => {
            println!("Nothing else to do, exiting!");
            return;
        }
    }
}
