//! libSQL / Turso persistence (default backend).

use std::path::{Path, PathBuf};
use std::sync::Arc;

use argon2::password_hash::{PasswordHash, PasswordHasher, PasswordVerifier, SaltString};
use argon2::Argon2;
use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use libsql::{params, params_from_iter, Builder, Connection, Database};
use password_hash::rand_core::OsRng;
use serde_json::{json, Map, Value as JsonValue};
use uuid::Uuid;

use crate::models::types::{AccessRule, EntityType, Field};
use crate::models::{AdminRow, EntitySchema};
use crate::util_time::now_rfc3339;

use super::catalog::{
    access_rule_from_document, apply_entity_catalog_patch, build_schema_document,
    entity_schema_from_document, fields_from_document, merge_rules_input_for_patch,
    migrate_catalog_libsql, preserve_extra_on_document,
};
use super::ddl::{build_create_table_ddl, ensure_entity_name, quote_ident};
use super::dir_migrate::apply_directory_sql_migrations_libsql;
use super::error::{Result, StorageError};
use super::migrate::apply_embedded_migrations;
use super::schema_alter::{plan_physical_ddl, SqlDialect};
use super::schema_migration::{
    format_schema_json_comment, join_physical_ddl_statements, try_write_generated_schema_migrations,
};

#[derive(Clone)]
pub struct LibsqlStore {
    db: Arc<Database>,
    migrations_dir: Arc<PathBuf>,
}

impl LibsqlStore {
    pub async fn open_local(
        data_dir: &str,
        db_url_override: &str,
        migrations_dir: impl AsRef<Path>,
    ) -> Result<Self> {
        std::fs::create_dir_all(data_dir)?;
        let path = if db_url_override.trim().is_empty() {
            Path::new(data_dir).join("mantis.db")
        } else {
            Path::new(db_url_override).to_path_buf()
        };
        let migrations_dir = migrations_dir.as_ref().to_path_buf();
        let db = Builder::new_local(path).build().await?;
        let s = Self {
            db: Arc::new(db),
            migrations_dir: Arc::new(migrations_dir),
        };
        apply_embedded_migrations(&s.db).await?;
        let c = s.conn()?;
        migrate_catalog_libsql(&c).await?;
        apply_directory_sql_migrations_libsql(&s.db, s.migrations_dir.as_path()).await?;
        Ok(s)
    }

    pub async fn open_remote(
        url: &str,
        auth_token: &str,
        migrations_dir: impl AsRef<Path>,
    ) -> Result<Self> {
        let migrations_dir = migrations_dir.as_ref().to_path_buf();
        let db = Builder::new_remote(url.to_string(), auth_token.to_string())
            .build()
            .await?;
        let s = Self {
            db: Arc::new(db),
            migrations_dir: Arc::new(migrations_dir),
        };
        apply_embedded_migrations(&s.db).await?;
        let c = s.conn()?;
        migrate_catalog_libsql(&c).await?;
        apply_directory_sql_migrations_libsql(&s.db, s.migrations_dir.as_path()).await?;
        Ok(s)
    }

    fn conn(&self) -> Result<Connection> {
        self.db.connect().map_err(Into::into)
    }

    pub async fn migrate(&self) -> Result<()> {
        apply_embedded_migrations(&self.db).await?;
        let c = self.conn()?;
        migrate_catalog_libsql(&c).await?;
        apply_directory_sql_migrations_libsql(&self.db, self.migrations_dir.as_path()).await
    }

    pub async fn verify_admin_basic(&self, email: &str, password: &str) -> Result<bool> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT password_hash, active FROM mb_admin WHERE email = ?",
                params![email],
            )
            .await?;
        let row = match rows.next().await? {
            Some(r) => r,
            None => return Ok(false),
        };
        let hash: String = row.get(0)?;
        let active: i64 = row.get(1)?;
        if active == 0 {
            return Ok(false);
        }
        let parsed =
            PasswordHash::new(&hash).map_err(|e| StorageError::Validation(e.to_string()))?;
        Ok(Argon2::default()
            .verify_password(password.as_bytes(), &parsed)
            .is_ok())
    }

    pub async fn get_admin_by_email(&self, email: &str) -> Result<Option<AdminRow>> {
        self.get_admin(email).await
    }

    pub async fn get_admin(&self, id_or_email: &str) -> Result<Option<AdminRow>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, email, active, password_reset_required FROM mb_admin WHERE id = ? OR email = ? LIMIT 1",
                params![id_or_email, id_or_email],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Ok(None);
        };
        Ok(Some(AdminRow {
            id: row.get(0)?,
            email: row.get(1)?,
            active: row.get::<i64>(2)? != 0,
            password_reset_required: row.get::<i64>(3)? != 0,
        }))
    }

    pub async fn list_admins(&self) -> Result<Vec<AdminRow>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, email, active, password_reset_required FROM mb_admin ORDER BY email",
                (),
            )
            .await?;
        let mut out = Vec::new();
        while let Some(row) = rows.next().await? {
            out.push(AdminRow {
                id: row.get(0)?,
                email: row.get(1)?,
                active: row.get::<i64>(2)? != 0,
                password_reset_required: row.get::<i64>(3)? != 0,
            });
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
            "INSERT INTO mb_admin (id, email, password_hash, created_at, updated_at, active, password_reset_required) VALUES (?, ?, ?, ?, ?, 1, 0)",
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
            .query(
                "SELECT document FROM mb_entity_schema WHERE name = ?",
                params![name],
            )
            .await?;
        let Some(r) = rows.next().await? else {
            return Ok(None);
        };
        let doc_s: String = r.get(0)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        Ok(doc
            .get("type")
            .and_then(|x| x.as_str())
            .map(std::string::ToString::to_string))
    }

    pub async fn list_entities_catalog(&self) -> Result<Vec<JsonValue>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, name, document FROM mb_entity_schema ORDER BY name",
                (),
            )
            .await?;
        let mut out = Vec::new();
        while let Some(row) = rows.next().await? {
            let id: String = row.get(0)?;
            let name: String = row.get(1)?;
            let doc_s: String = row.get(2)?;
            let doc: JsonValue = serde_json::from_str(&doc_s)?;
            out.push(super::catalog::list_row_from_document(&id, &name, &doc));
        }
        Ok(out)
    }

    pub async fn get_entity_catalog(&self, name: &str) -> Result<Option<JsonValue>> {
        ensure_entity_name(name)?;
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT document FROM mb_entity_schema WHERE name = ?",
                params![name],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Ok(None);
        };
        let doc_s: String = row.get(0)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        Ok(Some(super::catalog::full_catalog_json(name, &doc)))
    }

    pub async fn delete_entity_catalog(&self, name: &str) -> Result<()> {
        ensure_entity_name(name)?;
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT document FROM mb_entity_schema WHERE name = ?",
                params![name],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Err(StorageError::NotFound(name.to_string()));
        };
        let doc_s: String = row.get(0)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        let ty = doc.get("type").and_then(|x| x.as_str()).unwrap_or("bare");
        let preamble = format_schema_json_comment(&doc);
        if ty != "view" {
            let ddl = format!("DROP TABLE IF EXISTS {}", quote_ident(name)?);
            try_write_generated_schema_migrations(
                self.migrations_dir.as_path(),
                name,
                "drop",
                &preamble,
                &ddl,
                &ddl,
            );
        } else {
            let view_note = "-- view entity: no physical table to drop";
            try_write_generated_schema_migrations(
                self.migrations_dir.as_path(),
                name,
                "drop",
                &preamble,
                view_note,
                view_note,
            );
        }
        let q = format!("DROP TABLE IF EXISTS {}", quote_ident(name)?);
        conn.execute(&q, ()).await?;
        conn.execute("DELETE FROM mb_entity_schema WHERE name = ?", params![name])
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
        let mut schema = if let Some(f) = fields_override {
            let mut s = EntitySchema::new(name.to_string(), et);
            s.fields = f;
            s
        } else {
            EntitySchema::new(name.to_string(), et)
        };
        crate::models::normalize_fields(&mut schema.fields)
            .map_err(StorageError::Validation)?;
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
        let doc = build_schema_document(&entity_id, &schema, view_sql, rules);
        let doc_s = serde_json::to_string(&doc)?;
        conn.execute(
            "INSERT INTO mb_entity_schema (id, name, document, created_at, updated_at) VALUES (?, ?, ?, ?, ?)",
            params![
                entity_id.clone(),
                name,
                doc_s,
                ts.clone(),
                ts
            ],
        )
        .await?;
        if !matches!(et, EntityType::View) {
            let ddl = build_create_table_ddl(name, &schema.fields)?;
            conn.execute(&ddl, ()).await?;
            let preamble = format_schema_json_comment(&doc);
            try_write_generated_schema_migrations(
                self.migrations_dir.as_path(),
                name,
                "create",
                &preamble,
                &ddl,
                &ddl,
            );
        } else {
            let preamble = format_schema_json_comment(&doc);
            let view_note = "-- view entity: physical table not created";
            try_write_generated_schema_migrations(
                self.migrations_dir.as_path(),
                name,
                "create",
                &preamble,
                view_note,
                view_note,
            );
        }
        Ok(())
    }

    pub async fn patch_entity_catalog(
        &self,
        name: &str,
        patch: &super::EntityCatalogPatch,
    ) -> Result<()> {
        ensure_entity_name(name)?;
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT id, document FROM mb_entity_schema WHERE name = ?",
                params![name],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Err(StorageError::NotFound(name.to_string()));
        };
        let entity_id: String = row.get(0)?;
        let doc_s: String = row.get(1)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        let old_type = doc
            .get("type")
            .and_then(|x| x.as_str())
            .unwrap_or("bare")
            .to_string();
        let old_fields = fields_from_document(&doc)?;
        let mut schema = entity_schema_from_document(name, &doc)?;
        apply_entity_catalog_patch(&mut schema, patch)?;
        let rules_input = merge_rules_input_for_patch(&doc, patch.rules.as_ref());
        let view_sql_ref = schema.view_query.as_deref();
        let mut new_doc = build_schema_document(&entity_id, &schema, view_sql_ref, &rules_input);
        preserve_extra_on_document(&mut new_doc, &doc, patch);
        let new_type = new_doc
            .get("type")
            .and_then(|x| x.as_str())
            .unwrap_or("bare")
            .to_string();
        let new_fields = fields_from_document(&new_doc)?;
        let stmts_sqlite = plan_physical_ddl(
            name,
            &old_type,
            &old_fields,
            &new_type,
            &new_fields,
            SqlDialect::Sqlite,
        )?;
        let stmts_postgres = plan_physical_ddl(
            name,
            &old_type,
            &old_fields,
            &new_type,
            &new_fields,
            SqlDialect::Postgres,
        )?;
        let migration_body_sqlite = join_physical_ddl_statements(&stmts_sqlite);
        let migration_body_postgres = join_physical_ddl_statements(&stmts_postgres);
        conn.execute("BEGIN", ()).await?;
        let inner: Result<()> = async {
            for st in &stmts_sqlite {
                let t = st.trim();
                if t.is_empty() || t.starts_with("--") {
                    continue;
                }
                conn.execute(t, ()).await?;
            }
            let ts = now_rfc3339();
            let new_doc_s = serde_json::to_string(&new_doc)?;
            conn.execute(
                "UPDATE mb_entity_schema SET document = ?, updated_at = ? WHERE name = ?",
                params![new_doc_s, ts, name],
            )
            .await?;
            Ok(())
        }
        .await;
        match &inner {
            Ok(()) => {
                conn.execute("COMMIT", ()).await?;
            }
            Err(_) => {
                let _ = conn.execute("ROLLBACK", ()).await;
            }
        }
        inner?;
        let preamble = format!(
            "{}\n{}",
            format_schema_json_comment(&doc),
            format_schema_json_comment(&new_doc)
        );
        try_write_generated_schema_migrations(
            self.migrations_dir.as_path(),
            name,
            "patch",
            &preamble,
            &migration_body_sqlite,
            &migration_body_postgres,
        );
        Ok(())
    }

    pub async fn get_access_rule(&self, entity_name: &str, op: &str) -> Result<Option<AccessRule>> {
        let conn = self.conn()?;
        let mut rows = conn
            .query(
                "SELECT document FROM mb_entity_schema WHERE name = ?",
                params![entity_name],
            )
            .await?;
        let Some(row) = rows.next().await? else {
            return Ok(None);
        };
        let doc_s: String = row.get(0)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        Ok(access_rule_from_document(&doc, op))
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
