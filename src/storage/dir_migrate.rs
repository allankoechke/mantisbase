//! Optional `*.sql` files from the configured migrations directory, applied after embedded system DDL.
//! Tracked in `mb_sql_dir_migration` so each file runs once per database (skipped on later startups).

use std::fs;
use std::path::{Path, PathBuf};

use libsql::{params, Database};

use crate::util_time::now_rfc3339;

use super::error::{Result, StorageError};

/// Split SQL on `;` boundaries outside of single-quoted strings (basic, sufficient for typical migrations).
pub fn split_sql_statements(input: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    let mut in_single = false;
    let mut escaped = false;
    for ch in input.chars() {
        if escaped {
            cur.push(ch);
            escaped = false;
            continue;
        }
        if ch == '\\' && in_single {
            escaped = true;
            cur.push(ch);
            continue;
        }
        match ch {
            '\'' => {
                in_single = !in_single;
                cur.push(ch);
            }
            ';' if !in_single => {
                let s = cur.trim().to_string();
                if !s.is_empty() {
                    out.push(s);
                }
                cur.clear();
            }
            _ => cur.push(ch),
        }
    }
    let s = cur.trim().to_string();
    if !s.is_empty() {
        out.push(s);
    }
    out
}

fn validate_sql_leaf_name(name: &str) -> Result<()> {
    if name.is_empty() || name.contains("..") || name.contains('/') || name.contains('\\') {
        return Err(StorageError::Validation(format!(
            "invalid migration file name: {name}"
        )));
    }
    if !name.ends_with(".sql") {
        return Err(StorageError::Validation(format!(
            "migrations directory accepts only top-level .sql files: {name}"
        )));
    }
    let stem = name.trim_end_matches(".sql");
    if stem.is_empty()
        || !stem
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '-' || c == '.')
    {
        return Err(StorageError::Validation(format!(
            "migration file base name must be ASCII alphanumeric, dot, underscore, or hyphen: {name}"
        )));
    }
    Ok(())
}

fn list_sorted_sql_files(root: &Path) -> Result<Vec<PathBuf>> {
    if !root.exists() {
        fs::create_dir_all(root)?;
        return Ok(Vec::new());
    }
    let mut files: Vec<PathBuf> = fs::read_dir(root)?
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|p| p.is_file())
        .filter(|p| p.extension().is_some_and(|x| x == "sql"))
        .collect();
    files.sort();
    Ok(files)
}

/// Applies top-level `*.sql` from `root` in lexicographic order (after system catalog migrations).
pub async fn apply_directory_sql_migrations_libsql(db: &Database, root: &Path) -> Result<()> {
    let conn = db.connect()?;
    for path in list_sorted_sql_files(root)? {
        let fname = path
            .file_name()
            .and_then(|s| s.to_str())
            .ok_or_else(|| StorageError::Validation("migration path not UTF-8".into()))?;
        if fname == "001_initial.sql" {
            continue;
        }
        validate_sql_leaf_name(fname)?;
        let relpath = fname.to_string();
        let mut chk = conn
            .query(
                "SELECT 1 FROM mb_sql_dir_migration WHERE relpath = ? LIMIT 1",
                params![relpath.clone()],
            )
            .await?;
        if chk.next().await?.is_some() {
            continue;
        }
        let sql = fs::read_to_string(&path)?;
        if sql.trim().is_empty() {
            continue;
        }
        conn.execute_batch(&sql).await?;
        let ts = now_rfc3339();
        conn.execute(
            "INSERT INTO mb_sql_dir_migration (relpath, applied_at) VALUES (?, ?)",
            params![relpath, ts],
        )
        .await?;
    }
    Ok(())
}

pub async fn apply_directory_sql_migrations_postgres(
    pool: &sqlx::PgPool,
    root: &Path,
) -> Result<()> {
    for path in list_sorted_sql_files(root)? {
        let fname = path
            .file_name()
            .and_then(|s| s.to_str())
            .ok_or_else(|| StorageError::Validation("migration path not UTF-8".into()))?;
        if fname == "001_initial.sql" {
            continue;
        }
        validate_sql_leaf_name(fname)?;
        let relpath = fname.to_string();
        let applied: bool = sqlx::query_scalar(
            "SELECT EXISTS(SELECT 1 FROM mb_sql_dir_migration WHERE relpath = $1)",
        )
        .bind(&relpath)
        .fetch_one(pool)
        .await?;
        if applied {
            continue;
        }
        let sql = fs::read_to_string(&path)?;
        if sql.trim().is_empty() {
            continue;
        }
        let mut tx = pool.begin().await?;
        for stmt in split_sql_statements(&sql) {
            sqlx::query(&stmt).execute(&mut *tx).await?;
        }
        let ts = now_rfc3339();
        sqlx::query("INSERT INTO mb_sql_dir_migration (relpath, applied_at) VALUES ($1, $2)")
            .bind(&relpath)
            .bind(&ts)
            .execute(&mut *tx)
            .await?;
        tx.commit().await?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn split_respects_quotes() {
        let s = "SELECT ';'; SELECT 1;";
        let p = split_sql_statements(s);
        assert_eq!(p.len(), 2);
        assert!(p[0].contains("';'"));
    }
}
