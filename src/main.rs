use clap::Parser;
use mantisbase::cli::{Cli, Commands, DatabaseType};
use mantisbase::core::{MantisBase, MantisBaseDbType, MantisBaseMode};
use mantisbase::logger::prelude::*;
#[cfg(feature = "postgres")]
use mantisbase::storage::PostgresStore;
use mantisbase::storage::{LibsqlStore, Store};

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();

    let cli = Cli::parse();
    let mut mantis = MantisBase::new();

    if let Some(data_dir) = cli.data_dir {
        trace!("Data directory: {:?}", data_dir);
        mantis.set_data_dir(data_dir.to_str().unwrap_or("./data"));
    }
    if let Some(scripts_dir) = cli.scripts_dir {
        trace!("Scripts directory: {:?}", scripts_dir);
        mantis.set_scripts_dir(scripts_dir.to_str().unwrap_or("./scripts"));
    }
    if let Some(public_dir) = cli.public_dir {
        trace!("Public directory: {:?}", public_dir);
        mantis.set_public_dir(public_dir.to_str().unwrap_or("./public"));
    }
    if cli.dev {
        mantis.set_log_level(Level::Trace);
    }

    let db_type_cli = cli.db.unwrap_or(DatabaseType::Libsql);
    let mut db_url = cli.db_url.unwrap_or_default();

    let db_type = match db_type_cli {
        DatabaseType::Libsql => MantisBaseDbType::Libsql,
        DatabaseType::Turso => MantisBaseDbType::Turso,
        #[cfg(feature = "postgres")]
        DatabaseType::Postgresql => MantisBaseDbType::Postgres,
    };
    mantis.set_db_type(db_type);

    if db_url.trim().is_empty() {
        if let Ok(url) = std::env::var("MB_DATABASE_URL") {
            db_url = url;
        }
    }

    if db_type_cli != DatabaseType::Libsql && db_url.trim().is_empty() {
        panic!("Database URL required for Turso/Postgres (use --db-url or MB_DATABASE_URL)");
    }

    mantis.set_db_url(&db_url);

    match &cli.commands {
        Some(Commands::Serve(serve_args)) => {
            mantis.set_mode(MantisBaseMode::Serve);
            if let Some(p) = serve_args.port {
                mantis.set_port(p);
            }
            if let Some(h) = &serve_args.host {
                mantis.set_host(h);
            }
            let store = match db_type_cli {
                DatabaseType::Libsql => {
                    Store::Libsql(LibsqlStore::open_local(mantis.data_dir(), &db_url).await?)
                }
                DatabaseType::Turso => {
                    let token = std::env::var("MB_TURSO_AUTH_TOKEN")
                        .expect("MB_TURSO_AUTH_TOKEN required for Turso");
                    Store::Libsql(LibsqlStore::open_remote(&db_url, &token).await?)
                }
                #[cfg(feature = "postgres")]
                DatabaseType::Postgresql => Store::Postgres(PostgresStore::connect(&db_url).await?),
            };
            mantisbase::http::serve(&mantis, store).await?;
        }
        Some(Commands::Admins(admin_args)) => {
            mantis.set_mode(MantisBaseMode::Admin);
            let store = match db_type_cli {
                DatabaseType::Libsql | DatabaseType::Turso => {
                    Store::Libsql(LibsqlStore::open_local(mantis.data_dir(), &db_url).await?)
                }
                #[cfg(feature = "postgres")]
                DatabaseType::Postgresql => Store::Postgres(PostgresStore::connect(&db_url).await?),
            };
            if let Some(add_args) = &admin_args.add {
                if add_args.len() != 2 {
                    panic!("admins add requires EMAIL PASSWORD");
                }
                store.add_admin(&add_args[0], &add_args[1]).await?;
                println!("admin created");
            } else if admin_args.ls {
                for (id, email) in store.list_admins().await? {
                    println!("{id}\t{email}");
                }
            } else if let Some(target) = &admin_args.rm {
                let n = store.remove_admin(target).await?;
                println!("removed {n} row(s)");
            }
        }
        Some(Commands::Migrations(migration_args)) => {
            mantis.set_mode(MantisBaseMode::Migrations);
            let store = match db_type_cli {
                DatabaseType::Libsql | DatabaseType::Turso => {
                    Store::Libsql(LibsqlStore::open_local(mantis.data_dir(), &db_url).await?)
                }
                #[cfg(feature = "postgres")]
                DatabaseType::Postgresql => Store::Postgres(PostgresStore::connect(&db_url).await?),
            };
            if migration_args.up {
                store.migrate().await?;
                println!("migrations applied");
            } else if migration_args.reset {
                return Err(anyhow::anyhow!(
                    "destructive reset not implemented; remove data/mantis.db manually"
                ));
            } else {
                println!("use --up to apply migrations");
            }
        }
        None => {
            info!("Nothing to do — pass a subcommand (e.g. `serve`).");
        }
    }

    Ok(())
}
