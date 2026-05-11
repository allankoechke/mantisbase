//! libSQL / Turso persistence (default backend).

use std::path::Path;
use std::sync::Arc;

use argon2::password_hash::{PasswordHash, PasswordHasher, PasswordVerifier, SaltString};
use argon2::Argon2;
use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use libsql::{params, params_from_iter, Builder, Connection, Database};
use password_hash::rand_core::OsRng;
use serde_json::{json, Map, Value as JsonValue};
use uuid::Uuid;

use crate::models::types::{
    validate_entity_name, AccessMode, AccessRule, EntityType, Field, FieldType,
};
use crate::models::EntitySchema;
use crate::util_time::now_rfc3339;

use super::error::{Result, StorageError};
use super::migrate::apply_embedded_migrations;

fn field_type_sql(ft: &FieldType) -> &'static str {
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

fn ensure_entity_name(s: &str) -> Result<()> {
    validate_entity_name(s).map_err(|m| StorageError::Validation(m.to_string()))
}

fn quote_ident(name: &str) -> Result<String> {
    ensure_entity_name(name)?;
    Ok(format!("\"{}\"", name.replace('"', "")))
}

#[derive(Clone)]
pub struct LibsqlStore {
    db: Arc<Database>,
}

impl LibsqlStore {
    pub async fn open_local(data_dir: &str, db_url_override: &str) -> Result<Self> {
        std::fs::create_dir_all(data_dir)?;
        let path = if db_url_override.trim().is_empty() {
            Path::new(data_dir).join("mantis.db")
        } else {
            Path::new(db_url_override).to_path_buf()
        };
        let db = Builder::new_local(path).build().await?;
        let s = Self { db: Arc::new(db) };
        apply_embedded_migrations(&s.db).await?;
        Ok(s)
    }

    pub async fn open_remote(url: &str, auth_token: &str) -> Result<Self> {
        let db = Builder::new_remote(url.to_string(), auth_token.to_string())
            .build()
            .await?;
        let s = Self { db: Arc::new(db) };
        apply_embedded_migrations(&s.db).await?;
        Ok(s)
    }

    fn conn(&self) -> Result<Connection> {
        self.db.connect().map_err(Into::into)
    }

    pub async fn migrate(&self) -> Result<()> {
        apply_embedded_migrations(&self.db).await
    }

    pub async fn verify_admin_basic(&self, email: &str, password: &str) -> Result<bool> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT password_hash FROM mb_admin WHERE email = ?",
                params![email],
            )
            .await?;
        let row = match rows.next().await? {
            Some(r) => r,
            None => return Ok(false),
        };
        let hash: String = row.get(0)?;
        let parsed =
            PasswordHash::new(&hash).map_err(|e| StorageError::Validation(e.to_string()))?;
        Ok(Argon2::default()
            .verify_password(password.as_bytes(), &parsed)
            .is_ok())
    }

    pub async fn list_admins(&self) -> Result<Vec<(String, String)>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query("SELECT id, email FROM mb_admin ORDER BY email", ())
            .await?;
        let mut out = Vec::new();
        while let Some(row) = rows.next().await? {
            out.push((row.get(0)?, row.get(1)?));
        }
        Ok(out)
    }

    pub async fn add_admin(&self, email: &str, password: &str) -> Result<()> {
        let conn = self.conn()?;
        let id = Uuid::new_v4().to_string();
        let salt = SaltString::generate(&mut OsRng);
        let hash = Argon2::default()
            .hash_password(password.as_bytes(), &salt)
            .map_err(|e| StorageError::Validation(e.to_string()))?
            .to_string();
        let ts = now_rfc3339();
        conn.execute(
            "INSERT INTO mb_admin (id, email, password_hash, created_at, updated_at) VALUES (?, ?, ?, ?, ?)",
            params![id, email, hash, ts.clone(), ts],
        )
        .await?;
        Ok(())
    }

    pub async fn remove_admin(&self, id_or_email: &str) -> Result<u64> {
        let conn = self.conn()?;
        let n = conn
            .execute(
                "DELETE FROM mb_admin WHERE id = ? OR email = ?",
                params![id_or_email, id_or_email],
            )
            .await?;
        Ok(n)
    }

    pub async fn entity_type(&self, name: &str) -> Result<Option<String>> {
        ensure_entity_name(name)?;
        let conn = self.conn()?;
        let mut rows = conn
            .query("SELECT type FROM mb_entity WHERE name = ?", params![name])
            .await?;
        Ok(match rows.next().await? {
            Some(r) => Some(r.get::<String>(0)?),
            None => None,
        })
    }

    pub async fn list_entities_catalog(&self) -> Result<Vec<JsonValue>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, name, type, view_sql, is_system, has_api FROM mb_entity ORDER BY name",
                (),
            )
            .await?;
        let mut out = Vec::new();
        while let Some(row) = rows.next().await? {
            out.push(json!({
                "id": row.get::<String>(0)?,
                "name": row.get::<String>(1)?,
                "type": row.get::<String>(2)?,
                "view_sql": row.get::<Option<String>>(3)?,
                "is_system": row.get::<i64>(4)? != 0,
                "has_api": row.get::<i64>(5)? != 0,
            }));
        }
        Ok(out)
    }

    pub async fn get_entity_catalog(&self, name: &str) -> Result<Option<JsonValue>> {
        ensure_entity_name(name)?;
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, name, type, view_sql, is_system, has_api FROM mb_entity WHERE name = ?",
                params![name],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Ok(None);
        };
        let entity_id: String = row.get(0)?;
        let mut fields_rows = conn
            .query(
                "SELECT name, field_type, description, is_required, is_primary_key, is_unique, constraints_json, default_expr FROM mb_field WHERE entity_id = ? ORDER BY name",
                params![entity_id.clone()],
            )
            .await?;
        let mut fields = Vec::new();
        while let Some(fr) = fields_rows.next().await? {
            fields.push(json!({
                "name": fr.get::<String>(0)?,
                "type": fr.get::<String>(1)?,
                "description": fr.get::<Option<String>>(2)?,
                "required": fr.get::<i64>(3)? != 0,
                "primary_key": fr.get::<i64>(4)? != 0,
                "unique": fr.get::<i64>(5)? != 0,
                "constraints": fr.get::<Option<String>>(6)?,
                "default": fr.get::<Option<String>>(7)?,
            }));
        }
        let mut rules_rows = conn
            .query(
                "SELECT op, mode, expr FROM mb_access_rule WHERE entity_id = ?",
                params![entity_id],
            )
            .await?;
        let mut rules = serde_json::Map::new();
        while let Some(rr) = rules_rows.next().await? {
            let op: String = rr.get(0)?;
            let mode: String = rr.get(1)?;
            let expr: Option<String> = rr.get(2)?;
            rules.insert(
                op,
                json!({ "mode": mode, "expr": expr.unwrap_or_default() }),
            );
        }
        Ok(Some(json!({
            "name": row.get::<String>(1)?,
            "type": row.get::<String>(2)?,
            "view_sql": row.get::<Option<String>>(3)?,
            "is_system": row.get::<i64>(4)? != 0,
            "has_api": row.get::<i64>(5)? != 0,
            "fields": fields,
            "rules": rules,
        })))
    }

    pub async fn delete_entity_catalog(&self, name: &str) -> Result<()> {
        ensure_entity_name(name)?;
        let conn = self.conn()?;
        let mut rows = conn
            .query("SELECT id FROM mb_entity WHERE name = ?", params![name])
            .await?;
        let Some(_row) = rows.next().await? else {
            return Err(StorageError::NotFound(name.to_string()));
        };
        let q = format!("DROP TABLE IF EXISTS {}", quote_ident(name)?);
        conn.execute(&q, ()).await?;
        conn.execute("DELETE FROM mb_entity WHERE name = ?", params![name])
            .await?;
        Ok(())
    }

    pub async fn create_entity_from_schema(
        &self,
        name: &str,
        et: EntityType,
        view_sql: Option<&str>,
        fields_override: Option<Vec<Field>>,
        rules: &JsonValue,
    ) -> Result<()> {
        ensure_entity_name(name)?;
        let schema = if let Some(f) = fields_override {
            let mut s = EntitySchema::new(name.to_string(), et);
            s.fields = f;
            s
        } else {
            EntitySchema::new(name.to_string(), et)
        };
        if matches!(et, EntityType::View) {
            let vs = view_sql
                .ok_or_else(|| StorageError::Validation("view entity requires view_sql".into()))?;
            if vs.trim().is_empty() {
                return Err(StorageError::Validation("empty view_sql".into()));
            }
        }
        let conn = self.conn()?;
        let entity_id = Uuid::new_v4().to_string();
        let ts = now_rfc3339();
        let type_str = match et {
            EntityType::Bare => "bare",
            EntityType::Base => "base",
            EntityType::View => "view",
        };
        conn.execute(
            "INSERT INTO mb_entity (id, name, type, view_sql, is_system, has_api, created_at, updated_at) VALUES (?, ?, ?, ?, 0, 1, ?, ?)",
            params![
                entity_id.clone(),
                name,
                type_str,
                view_sql,
                ts.clone(),
                ts
            ],
        )
        .await?;
        for f in &schema.fields {
            let fid = Uuid::new_v4().to_string();
            let ft = field_type_to_catalog_str(&f.field_type);
            conn.execute(
                "INSERT INTO mb_field (id, entity_id, name, field_type, description, is_required, is_primary_key, is_unique, constraints_json, default_expr) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                params![
                    fid,
                    entity_id.clone(),
                    f.field_name.clone(),
                    ft,
                    f.field_description.clone(),
                    f.is_required as i64,
                    f.is_primary_key as i64,
                    f.is_unique as i64,
                    f.constraints.clone(),
                    f.default.clone(),
                ],
            )
            .await?;
        }
        for (op, default_rule) in [
            ("list", AccessRule::admin_only()),
            ("read", AccessRule::admin_only()),
            ("create", AccessRule::admin_only()),
            ("update", AccessRule::admin_only()),
            ("delete", AccessRule::admin_only()),
        ] {
            let r = parse_rule_from_json(rules, op).unwrap_or(default_rule);
            conn.execute(
                "INSERT INTO mb_access_rule (entity_id, op, mode, expr, dsl_version) VALUES (?, ?, ?, ?, 1)",
                params![entity_id.clone(), op, r.mode_str(), r.expr],
            )
            .await?;
        }
        if !matches!(et, EntityType::View) {
            let ddl = build_create_table_ddl(name, &schema.fields)?;
            conn.execute(&ddl, ()).await?;
        }
        Ok(())
    }

    pub async fn get_access_rule(&self, entity_name: &str, op: &str) -> Result<Option<AccessRule>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT r.mode, r.expr FROM mb_access_rule r JOIN mb_entity e ON e.id = r.entity_id WHERE e.name = ? AND r.op = ?",
                params![entity_name, op],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Ok(None);
        };
        let mode: String = row.get(0)?;
        let expr: Option<String> = row.get(1)?;
        Ok(Some(AccessRule::from_db(&mode, expr)))
    }

    pub async fn list_entity_rows(
        &self,
        entity: &str,
        limit: u32,
        offset: u32,
    ) -> Result<Vec<Map<String, JsonValue>>> {
        ensure_entity_name(entity)?;
        let conn = self.conn()?;
        let q = format!("SELECT * FROM {} LIMIT ? OFFSET ?", quote_ident(entity)?);
        let mut rows = conn.query(&q, params![limit as i64, offset as i64]).await?;
        let mut out = Vec::new();
        while let Some(row) = rows.next().await? {
            let n = row.column_count();
            let mut m = Map::new();
            for i in 0..n {
                let name = row
                    .column_name(i)
                    .ok_or_else(|| StorageError::Validation("column name".into()))?;
                let v = row.get_value(i)?;
                m.insert(name.to_string(), libsql_value_to_json(v)?);
            }
            out.push(m);
        }
        Ok(out)
    }

    pub async fn get_entity_row(
        &self,
        entity: &str,
        id: &str,
    ) -> Result<Option<Map<String, JsonValue>>> {
        ensure_entity_name(entity)?;
        let conn = self.conn()?;
        let q = format!("SELECT * FROM {} WHERE id = ?", quote_ident(entity)?);
        let mut rows = conn.query(&q, params![id]).await?;
        let Some(row) = rows.next().await? else {
            return Ok(None);
        };
        let n = row.column_count();
        let mut m = Map::new();
        for i in 0..n {
            let name = row
                .column_name(i)
                .ok_or_else(|| StorageError::Validation("column name".into()))?;
            let v = row.get_value(i)?;
            m.insert(name.to_string(), libsql_value_to_json(v)?);
        }
        Ok(Some(m))
    }

    pub async fn insert_entity_row(
        &self,
        entity: &str,
        mut body: Map<String, JsonValue>,
    ) -> Result<Map<String, JsonValue>> {
        ensure_entity_name(entity)?;
        for k in body.keys() {
            ensure_entity_name(k)?;
        }
        let now = now_rfc3339();
        if !body.contains_key("id") {
            body.insert("id".into(), json!(Uuid::new_v4().to_string()));
        }
        body.entry("created_at")
            .or_insert_with(|| json!(now.clone()));
        body.entry("updated_at").or_insert_with(|| json!(now));
        let keys: Vec<String> = body.keys().cloned().collect();
        let placeholders = (0..keys.len())
            .map(|_| "?".to_string())
            .collect::<Vec<_>>()
            .join(", ");
        let cols = keys
            .iter()
            .map(|k| quote_ident(k))
            .collect::<Result<Vec<_>>>()?
            .join(",");
        let sql = format!(
            "INSERT INTO {} ({}) VALUES ({})",
            quote_ident(entity)?,
            cols,
            placeholders
        );
        let conn = self.conn()?;
        let params_vec: Vec<libsql::Value> = keys
            .iter()
            .map(|k| json_to_libsql(body.get(k).unwrap_or(&JsonValue::Null)))
            .collect();
        conn.execute(&sql, params_from_iter(params_vec)).await?;
        let id = body.get("id").and_then(|v| v.as_str()).unwrap_or("");
        self.get_entity_row(entity, id)
            .await?
            .ok_or_else(|| StorageError::NotFound(id.to_string()))
    }

    pub async fn update_entity_row(
        &self,
        entity: &str,
        id: &str,
        patch: Map<String, JsonValue>,
    ) -> Result<Map<String, JsonValue>> {
        ensure_entity_name(entity)?;
        if patch.contains_key("id") {
            return Err(StorageError::Validation("cannot change id".into()));
        }
        for k in patch.keys() {
            ensure_entity_name(k)?;
        }
        let mut assignments = Vec::new();
        let mut params_vec: Vec<libsql::Value> = Vec::new();
        for (k, v) in patch {
            if k == "created_at" {
                continue;
            }
            assignments.push(format!("{} = ?", quote_ident(&k)?));
            params_vec.push(json_to_libsql(&v));
        }
        assignments.push(format!("{} = ?", quote_ident("updated_at")?));
        params_vec.push(json_to_libsql(&json!(now_rfc3339())));
        params_vec.push(json_to_libsql(&json!(id)));
        let sql = format!(
            "UPDATE {} SET {} WHERE id = ?",
            quote_ident(entity)?,
            assignments.join(", ")
        );
        let conn = self.conn()?;
        let n = conn.execute(&sql, params_from_iter(params_vec)).await?;
        if n == 0 {
            return Err(StorageError::NotFound(id.to_string()));
        }
        self.get_entity_row(entity, id)
            .await?
            .ok_or_else(|| StorageError::NotFound(id.to_string()))
    }

    pub async fn delete_entity_row(&self, entity: &str, id: &str) -> Result<()> {
        ensure_entity_name(entity)?;
        let conn = self.conn()?;
        let q = format!("DELETE FROM {} WHERE id = ?", quote_ident(entity)?);
        let n = conn.execute(&q, params![id]).await?;
        if n == 0 {
            return Err(StorageError::NotFound(id.to_string()));
        }
        Ok(())
    }

    /// Validates credentials against the central `mb_user` table (not catalog entities).
    pub async fn verify_user_login(
        &self,
        email: &str,
        password: &str,
    ) -> Result<Option<Map<String, JsonValue>>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, email, profile_json, created_at, updated_at, password_hash FROM mb_user WHERE email = ? LIMIT 1",
                params![email],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Ok(None);
        };
        let hash: String = row.get(5)?;
        let parsed =
            PasswordHash::new(&hash).map_err(|e| StorageError::Validation(e.to_string()))?;
        if Argon2::default()
            .verify_password(password.as_bytes(), &parsed)
            .is_err()
        {
            return Ok(None);
        }
        let mut m = Map::new();
        m.insert("id".into(), json!(row.get::<String>(0)?));
        m.insert("email".into(), json!(row.get::<String>(1)?));
        m.insert(
            "profile".into(),
            match row.get::<Option<String>>(2)? {
                Some(s) if !s.trim().is_empty() => serde_json::from_str(&s).unwrap_or(json!(s)),
                _ => JsonValue::Null,
            },
        );
        m.insert("created_at".into(), json!(row.get::<String>(3)?));
        m.insert("updated_at".into(), json!(row.get::<String>(4)?));
        Ok(Some(m))
    }

    pub async fn app_config_get(&self, key: &str) -> Result<Option<String>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT value FROM mb_app_config WHERE key = ?",
                params![key],
            )
            .await?;
        Ok(match rows.next().await? {
            Some(r) => Some(r.get::<String>(0)?),
            None => None,
        })
    }

    pub async fn app_config_set(
        &self,
        key: &str,
        value: &str,
        updated_by: Option<&str>,
    ) -> Result<()> {
        let conn = self.conn()?;
        let ts = now_rfc3339();
        conn.execute(
            "INSERT INTO mb_app_config (key, value, updated_at, updated_by) VALUES (?, ?, ?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at, updated_by = excluded.updated_by",
            params![key, value, ts, updated_by],
        )
        .await?;
        Ok(())
    }

    pub async fn app_config_list(&self) -> Result<Vec<(String, String)>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query("SELECT key, value FROM mb_app_config ORDER BY key", ())
            .await?;
        let mut out = Vec::new();
        while let Some(row) = rows.next().await? {
            out.push((row.get(0)?, row.get(1)?));
        }
        Ok(out)
    }

    pub async fn register_webhook(
        &self,
        url: &str,
        events: &[String],
        secret: Option<&str>,
    ) -> Result<String> {
        let id = Uuid::new_v4().to_string();
        let conn = self.conn()?;
        let ev = serde_json::to_string(events)?;
        let ts = now_rfc3339();
        conn.execute(
            "INSERT INTO mb_webhook (id, url, secret_hmac, events, enabled, created_at) VALUES (?, ?, ?, ?, 1, ?)",
            params![id.clone(), url, secret, ev, ts],
        )
        .await?;
        Ok(id)
    }

    pub async fn list_webhooks(&self) -> Result<Vec<JsonValue>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, url, events, enabled, created_at FROM mb_webhook ORDER BY created_at",
                (),
            )
            .await?;
        let mut out = Vec::new();
        while let Some(row) = rows.next().await? {
            out.push(json!({
                "id": row.get::<String>(0)?,
                "url": row.get::<String>(1)?,
                "events": serde_json::from_str::<JsonValue>(&row.get::<String>(2)?)?,
                "enabled": row.get::<i64>(3)? != 0,
                "created_at": row.get::<String>(4)?,
            }));
        }
        Ok(out)
    }
}

pub(crate) fn field_type_to_catalog_str(ft: &FieldType) -> &'static str {
    match ft {
        FieldType::String => "string",
        FieldType::Text => "text",
        FieldType::Password => "password",
        FieldType::Int => "int",
        FieldType::Float => "float",
        FieldType::Bool => "bool",
        FieldType::DateTime => "datetime",
        FieldType::Json => "json",
    }
}

pub(crate) fn parse_rule_from_json(rules: &JsonValue, op: &str) -> Option<AccessRule> {
    let obj = rules.as_object()?.get("rules")?.as_object()?;
    let legacy = match op {
        "read" => obj.get("get"),
        _ => obj.get(op),
    };
    let v = legacy.or_else(|| obj.get(op))?;
    let mode_raw = v.get("mode")?.as_str()?.to_string();
    let expr = v
        .get("expr")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .to_string();
    let mode = match mode_raw.as_str() {
        "public" => AccessMode::Public,
        "admin" => AccessMode::Admin,
        "auth" | "authenticated" => AccessMode::Authenticated,
        "custom" => AccessMode::Custom,
        _ => AccessMode::Admin,
    };
    Some(AccessRule { mode, expr })
}

fn build_create_table_ddl(table: &str, fields: &[Field]) -> Result<String> {
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

fn libsql_value_to_json(v: libsql::Value) -> Result<JsonValue> {
    Ok(match v {
        libsql::Value::Null => JsonValue::Null,
        libsql::Value::Integer(i) => json!(i),
        libsql::Value::Real(x) => json!(x),
        libsql::Value::Text(s) => {
            if let Ok(j) = serde_json::from_str::<JsonValue>(&s) {
                j
            } else {
                json!(s)
            }
        }
        libsql::Value::Blob(b) => json!(B64.encode(b)),
    })
}

fn json_to_libsql(v: &JsonValue) -> libsql::Value {
    match v {
        JsonValue::Null => libsql::Value::Null,
        JsonValue::Bool(b) => libsql::Value::Integer(if *b { 1 } else { 0 }),
        JsonValue::Number(n) => {
            if let Some(i) = n.as_i64() {
                libsql::Value::Integer(i)
            } else if let Some(u) = n.as_u64() {
                libsql::Value::Integer(u as i64)
            } else {
                libsql::Value::Real(n.as_f64().unwrap_or(0.0))
            }
        }
        JsonValue::String(s) => libsql::Value::Text(s.clone()),
        JsonValue::Array(_) | JsonValue::Object(_) => libsql::Value::Text(v.to_string()),
    }
}
