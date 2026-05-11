use libsql::Database;

use super::error::Result;

const INITIAL_SQL: &str = include_str!("../../migrations/001_initial.sql");

/// Applies embedded DDL (idempotent `IF NOT EXISTS` + migration ledger insert).
pub async fn apply_embedded_migrations(db: &Database) -> Result<()> {
    let conn = db.connect()?;
    conn.execute("PRAGMA foreign_keys = ON", ()).await?;
    conn.execute_batch(INITIAL_SQL).await?;
    Ok(())
}
