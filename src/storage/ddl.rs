//! Shared DDL helpers for catalog-backed physical tables (libSQL + PostgreSQL).

use crate::models::types::{validate_entity_name, Field, FieldType};

use super::error::{Result, StorageError};

pub fn ensure_entity_name(s: &str) -> Result<()> {
    validate_entity_name(s).map_err(|m| StorageError::Validation(m.to_string()))
}

pub fn quote_ident(name: &str) -> Result<String> {
    ensure_entity_name(name)?;
    Ok(format!("\"{}\"", name.replace('"', "")))
}

pub fn field_type_sql(ft: &FieldType) -> &'static str {
    match ft {
        FieldType::String
        | FieldType::Text
        | FieldType::Password
        | FieldType::Json
        | FieldType::DateTime => "TEXT",
        FieldType::Int | FieldType::Bool => "INTEGER",
        FieldType::Float => "REAL",
    }
}

pub fn build_create_table_ddl(table: &str, fields: &[Field]) -> Result<String> {
    let mut parts = Vec::new();
    for f in fields {
        ensure_entity_name(&f.field_name)?;
        let nullsql = if f.is_required { "NOT NULL" } else { "NULL" };
        let mut col = format!(
            "{} {} {}",
            quote_ident(&f.field_name)?,
            field_type_sql(&f.field_type),
            nullsql
        );
        if f.is_primary_key {
            col.push_str(" PRIMARY KEY");
        }
        if f.is_unique && !f.is_primary_key {
            col.push_str(" UNIQUE");
        }
        parts.push(col);
    }
    Ok(format!(
        "CREATE TABLE IF NOT EXISTS {} ({})",
        quote_ident(table)?,
        parts.join(", ")
    ))
}
