use libsql::Database;

use super::error::Result;

const INITIAL_SQL: &str = include_str!("../../migrations/001_initial.sql");

/// Applies embedded system DDL (`migrations/001_initial.sql`): idempotent catalog bootstrap for libSQL/Turso.
pub async fn apply_embedded_migrations(db: &Database) -> Result<()> {
    let conn = db.connect()?;
    conn.execute("PRAGMA foreign_keys = ON", ()).await?;
    conn.execute_batch(INITIAL_SQL).await?;
    Ok(())
}
