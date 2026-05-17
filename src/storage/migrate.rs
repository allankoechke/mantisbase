use libsql::Database;

use super::error::Result;

const INITIAL_SQL: &str = include_str!("../../migrations/001_initial.sql");

/// Adds `active` / `password_reset_required` to `mb_admin` when upgrading from older embedded DDL.
async fn ensure_mb_admin_columns(conn: &libsql::Connection) -> Result<()> {
    let mut rows = conn.query("PRAGMA table_info(mb_admin)", ()).await?;
    let mut has_active = false;
    let mut has_pwd_reset = false;
    while let Some(r) = rows.next().await? {
        let name: String = r.get(1)?;
        match name.as_str() {
            "active" => has_active = true,
            "password_reset_required" => has_pwd_reset = true,
            _ => {}
        }
    }
    if !has_active {
        conn.execute(
            "ALTER TABLE mb_admin ADD COLUMN active INTEGER NOT NULL DEFAULT 1",
            (),
        )
        .await?;
    }
    if !has_pwd_reset {
        conn.execute(
            "ALTER TABLE mb_admin ADD COLUMN password_reset_required INTEGER NOT NULL DEFAULT 0",
            (),
        )
        .await?;
    }
    Ok(())
}

/// Applies embedded system DDL (`migrations/001_initial.sql`): idempotent catalog bootstrap for libSQL/Turso.
pub async fn apply_embedded_migrations(db: &Database) -> Result<()> {
    let conn = db.connect()?;
    conn.execute("PRAGMA foreign_keys = ON", ()).await?;
    conn.execute_batch(INITIAL_SQL).await?;
    ensure_mb_admin_columns(&conn).await?;
    Ok(())
}
