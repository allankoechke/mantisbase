//! Column-level DDL for catalog patches: `ADD` / `DROP` / `ALTER` (Postgres) vs SQLite limits.

use std::collections::{HashMap, HashSet};

use crate::models::types::Field;
use crate::models::types::FieldType;
use uuid::Uuid;

use super::ddl::{
    build_create_table_ddl, build_create_table_ddl_plain, ensure_entity_name, field_type_sql,
    quote_ident,
};
use super::error::{Result, StorageError};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SqlDialect {
    Sqlite,
    Postgres,
}

fn field_physical_eq(a: &Field, b: &Field) -> bool {
    a.field_name == b.field_name
        && a.field_type == b.field_type
        && a.is_required == b.is_required
        && a.is_primary_key == b.is_primary_key
        && a.is_unique == b.is_unique
        && a.default == b.default
        && a.constraints == b.constraints
}

fn field_maps(fields: &[Field]) -> HashMap<String, Field> {
    fields
        .iter()
        .map(|f| (f.field_name.clone(), f.clone()))
        .collect()
}

fn sql_string_literal(s: &str) -> String {
    format!("'{}'", s.replace('\'', "''"))
}

/// Default clause for `ADD COLUMN` when the column is `NOT NULL` and SQLite needs a constant for existing rows.
fn add_column_default_suffix(f: &Field) -> String {
    if let Some(d) = f.default.as_deref().filter(|x| !x.is_empty()) {
        return format!(" DEFAULT {}", default_sql_expr(d, &f.field_type));
    }
    match f.field_type {
        FieldType::Int | FieldType::Bool => " DEFAULT 0".into(),
        FieldType::Float => " DEFAULT 0.0".into(),
        _ => " DEFAULT ''".into(),
    }
}

fn default_sql_expr(raw: &str, ft: &FieldType) -> String {
    match ft {
        FieldType::Int => raw
            .parse::<i64>()
            .map(|i| i.to_string())
            .unwrap_or_else(|_| "0".into()),
        FieldType::Bool => {
            if raw == "1" || raw.eq_ignore_ascii_case("true") {
                "1".into()
            } else {
                "0".into()
            }
        }
        FieldType::Float => raw
            .parse::<f64>()
            .map(|x| x.to_string())
            .unwrap_or_else(|_| "0.0".into()),
        _ => sql_string_literal(raw),
    }
}

fn add_column_stmt(table: &str, f: &Field, dialect: SqlDialect) -> Result<String> {
    if f.is_primary_key {
        return Err(StorageError::Validation(
            "cannot ADD COLUMN with PRIMARY KEY on an existing table".into(),
        ));
    }
    let qtable = quote_ident(table)?;
    let qcol = quote_ident(&f.field_name)?;
    let typ = field_type_sql(&f.field_type);
    let nullsql = if f.is_required {
        format!("NOT NULL{}", add_column_default_suffix(f))
    } else {
        "NULL".into()
    };
    let uniq = if f.is_unique && !f.is_primary_key {
        " UNIQUE"
    } else {
        ""
    };
    Ok(match dialect {
        SqlDialect::Sqlite | SqlDialect::Postgres => {
            format!("ALTER TABLE {qtable} ADD COLUMN {qcol} {typ} {nullsql}{uniq}")
        }
    })
}

fn drop_column_stmt(table: &str, col: &str, dialect: SqlDialect) -> Result<String> {
    let qtable = quote_ident(table)?;
    let qcol = quote_ident(col)?;
    Ok(match dialect {
        SqlDialect::Sqlite => format!("ALTER TABLE {qtable} DROP COLUMN {qcol}"),
        SqlDialect::Postgres => format!("ALTER TABLE {qtable} DROP COLUMN {qcol} CASCADE"),
    })
}

fn pg_alter_column_chunks(table: &str, old: &Field, new: &Field) -> Result<Vec<String>> {
    let qtable = quote_ident(table)?;
    let qcol = quote_ident(&new.field_name)?;
    let mut parts = Vec::new();
    if old.field_type != new.field_type {
        let new_pg = field_type_sql(&new.field_type);
        parts.push(format!(
            "ALTER TABLE {qtable} ALTER COLUMN {qcol} TYPE {new_pg} USING ({qcol}::{new_pg})"
        ));
    }
    if old.is_required != new.is_required {
        if new.is_required {
            parts.push(format!(
                "ALTER TABLE {qtable} ALTER COLUMN {qcol} SET NOT NULL"
            ));
        } else {
            parts.push(format!(
                "ALTER TABLE {qtable} ALTER COLUMN {qcol} DROP NOT NULL"
            ));
        }
    }
    if old.default != new.default {
        match &new.default {
            Some(d) if !d.trim().is_empty() => {
                parts.push(format!(
                    "ALTER TABLE {qtable} ALTER COLUMN {qcol} SET DEFAULT {}",
                    default_sql_expr(d, &new.field_type)
                ));
            }
            _ => {
                parts.push(format!(
                    "ALTER TABLE {qtable} ALTER COLUMN {qcol} DROP DEFAULT"
                ));
            }
        }
    }
    if old.is_unique != new.is_unique {
        return Err(StorageError::Validation(
            "changing UNIQUE via ALTER is not automated; add a named constraint migration manually"
                .into(),
        ));
    }
    if old.is_primary_key != new.is_primary_key {
        return Err(StorageError::Validation(
            "cannot change PRIMARY KEY flag on an existing column".into(),
        ));
    }
    Ok(parts)
}

fn schemas_physically_equal(old_fields: &[Field], new_fields: &[Field]) -> bool {
    let om = field_maps(old_fields);
    let nm = field_maps(new_fields);
    if om.len() != nm.len() {
        return false;
    }
    for (k, nf) in &nm {
        match om.get(k) {
            Some(of) if field_physical_eq(of, nf) => {}
            _ => return false,
        }
    }
    true
}

/// Expression for a new column in `INSERT ... SELECT` when the source row has no such column.
fn insert_default_expr(f: &Field) -> String {
    if let Some(d) = f.default.as_deref().filter(|x| !x.is_empty()) {
        return default_sql_expr(d, &f.field_type);
    }
    match f.field_type {
        FieldType::Int | FieldType::Bool => "0".into(),
        FieldType::Float => "0.0".into(),
        _ => "''".into(),
    }
}

fn sqlite_rebuild_table_stmts(
    table: &str,
    old_fields: &[Field],
    new_fields: &[Field],
) -> Result<Vec<String>> {
    let staging = format!("_{}", Uuid::new_v4().as_simple());
    ensure_entity_name(&staging)?;
    let q_table = quote_ident(table)?;
    let q_staging = quote_ident(&staging)?;
    let mut out = Vec::new();
    out.push(format!(
        "{};",
        build_create_table_ddl_plain(&staging, new_fields)?
    ));
    let old_names: HashSet<String> = old_fields.iter().map(|f| f.field_name.clone()).collect();
    let mut col_list = Vec::with_capacity(new_fields.len());
    let mut select_parts = Vec::with_capacity(new_fields.len());
    for f in new_fields {
        col_list.push(quote_ident(&f.field_name)?);
        let part = if old_names.contains(&f.field_name) {
            quote_ident(&f.field_name)?
        } else if f.is_required {
            insert_default_expr(f)
        } else {
            "NULL".into()
        };
        select_parts.push(part);
    }
    out.push(format!(
        "INSERT INTO {} ({}) SELECT {} FROM {};",
        q_staging,
        col_list.join(", "),
        select_parts.join(", "),
        q_table
    ));
    out.push(format!("DROP TABLE {q_table};"));
    out.push(format!(
        "ALTER TABLE {q_staging} RENAME TO {};",
        quote_ident(table)?
    ));
    Ok(out)
}

/// Plans DDL for physical table `table` when catalog type/fields change.
pub fn plan_physical_ddl(
    table: &str,
    old_type: &str,
    old_fields: &[Field],
    new_type: &str,
    new_fields: &[Field],
    dialect: SqlDialect,
) -> Result<Vec<String>> {
    let mut out = Vec::new();
    if old_type != "view" && new_type == "view" {
        out.push(format!("DROP TABLE IF EXISTS {};", quote_ident(table)?));
        return Ok(out);
    }
    if new_type == "view" {
        return Ok(out);
    }
    if old_type == "view" && new_type != "view" {
        out.push(build_create_table_ddl(table, new_fields)?);
        return Ok(out);
    }
    if dialect == SqlDialect::Sqlite {
        if schemas_physically_equal(old_fields, new_fields) {
            return Ok(vec![]);
        }
        return sqlite_rebuild_table_stmts(table, old_fields, new_fields);
    }
    let old_m = field_maps(old_fields);
    let new_m = field_maps(new_fields);
    for name in new_m.keys() {
        if !old_m.contains_key(name) {
            if new_m[name].is_primary_key {
                return Err(StorageError::Validation(format!(
                    "new column `{name}` cannot declare PRIMARY KEY on an existing table"
                )));
            }
            out.push(format!(
                "{};",
                add_column_stmt(table, &new_m[name], dialect)?
            ));
        }
    }
    for name in old_m.keys() {
        if !new_m.contains_key(name) {
            out.push(format!("{};", drop_column_stmt(table, name, dialect)?));
        }
    }
    for name in new_m.keys() {
        if let (Some(o), Some(n)) = (old_m.get(name), new_m.get(name)) {
            if field_physical_eq(o, n) {
                continue;
            }
            if o.is_primary_key != n.is_primary_key {
                return Err(StorageError::Validation(format!(
                    "cannot change PRIMARY KEY on column `{name}`"
                )));
            }
            for chunk in pg_alter_column_chunks(table, o, n)? {
                out.push(format!("{chunk};"));
            }
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::models::types::FieldType;

    fn col(name: &str, ft: FieldType) -> Field {
        Field {
            field_id: name.to_string(),
            field_name: name.to_string(),
            field_description: None,
            field_type: ft,
            is_required: false,
            is_primary_key: false,
            is_unique: false,
            constraints: None,
            default: None,
        }
    }

    #[test]
    fn sqlite_plan_rebuilds_on_add_column() {
        let old = vec![col("id", FieldType::String), col("x", FieldType::Int)];
        let mut new = old.clone();
        new.push(col("note", FieldType::Text));
        let stmts = plan_physical_ddl("t", "base", &old, "base", &new, SqlDialect::Sqlite).unwrap();
        let joined = stmts.join("\n");
        assert!(joined.contains("CREATE TABLE "));
        assert!(joined.contains("INSERT INTO"));
        assert!(joined.contains("DROP TABLE"));
        assert!(joined.contains("RENAME TO"));
        assert!(!joined.contains("ADD COLUMN"));
    }

    #[test]
    fn sqlite_plan_rebuilds_on_type_change() {
        let old = vec![col("id", FieldType::String), col("n", FieldType::String)];
        let mut new = old.clone();
        new[1] = Field {
            field_type: FieldType::Int,
            ..new[1].clone()
        };
        let stmts = plan_physical_ddl("t", "bare", &old, "bare", &new, SqlDialect::Sqlite).unwrap();
        let joined = stmts.join("\n");
        assert!(joined.contains("INSERT INTO"));
        assert!(joined.contains("RENAME TO"));
    }

    #[test]
    fn sqlite_plan_noop_when_equal() {
        let old = vec![col("id", FieldType::String)];
        let stmts = plan_physical_ddl("t", "base", &old, "base", &old, SqlDialect::Sqlite).unwrap();
        assert!(stmts.is_empty());
    }

    #[test]
    fn postgres_plan_includes_alter_type() {
        let old = vec![col("id", FieldType::String), col("n", FieldType::String)];
        let mut new = old.clone();
        new[1] = Field {
            field_type: FieldType::Int,
            ..new[1].clone()
        };
        let stmts =
            plan_physical_ddl("t", "bare", &old, "bare", &new, SqlDialect::Postgres).unwrap();
        assert!(stmts.iter().any(|s| s.contains("ALTER COLUMN")));
    }
}
