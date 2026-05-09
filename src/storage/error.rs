use thiserror::Error;

#[derive(Debug, Error)]
pub enum StorageError {
    #[error("libsql: {0}")]
    Libsql(#[from] libsql::Error),
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
    #[error("serde: {0}")]
    Serde(#[from] serde_json::Error),
    #[error("not found: {0}")]
    NotFound(String),
    #[error("conflict: {0}")]
    Conflict(String),
    #[error("validation: {0}")]
    Validation(String),
    #[error("postgres backend not compiled in")]
    PostgresDisabled,
    #[cfg(feature = "postgres")]
    #[error("sqlx: {0}")]
    Sqlx(#[from] sqlx::Error),
    #[cfg(feature = "postgres")]
    #[error("sqlx migrate: {0}")]
    SqlxMigrate(#[from] sqlx::migrate::MigrateError),
}

pub type Result<T> = std::result::Result<T, StorageError>;
