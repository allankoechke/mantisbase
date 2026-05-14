//! PostgreSQL persistence. Mirrors [`super::LibsqlStore`] API.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use argon2::password_hash::{PasswordHash, PasswordHasher, PasswordVerifier, SaltString};
use argon2::Argon2;
use password_hash::rand_core::OsRng;
use serde_json::{json, Map, Value as JsonValue};
use sqlx::postgres::PgRow;
use sqlx::{Column, PgPool, QueryBuilder, Row};
use uuid::Uuid;

use crate::models::types::{AccessRule, EntityType, Field};
use crate::models::EntitySchema;
use crate::util_time::now_rfc3339;

use super::catalog::{
    access_rule_from_document, apply_entity_catalog_patch, build_schema_document,
    entity_schema_from_document, fields_from_document, merge_rules_input_for_patch,
    migrate_catalog_postgres, preserve_extra_on_document,
};
use super::ddl::{build_create_table_ddl, ensure_entity_name, quote_ident};
use super::dir_migrate::apply_directory_sql_migrations_postgres;
use super::error::{Result, StorageError};
use super::schema_alter::{plan_physical_ddl, SqlDialect};
use super::schema_migration::{
    format_schema_json_comment, join_physical_ddl_statements, try_write_generated_schema_migrations,
};

#[derive(Clone)]
pub struct PostgresStore {
    pool: PgPool,
    migrations_dir: Arc<PathBuf>,
}

impl PostgresStore {
    pub async fn connect(
        database_url: &str,
        migrations_dir: impl AsRef<Path>,
    ) -> Result<Self> {
        let migrations_dir = migrations_dir.as_ref().to_path_buf();
        let pool = sqlx::postgres::PgPoolOptions::new()
            .max_connections(10)
            .connect(database_url)
            .await?;
        sqlx::migrate!("migrations/postgres").run(&pool).await?;
        migrate_catalog_postgres(&pool).await?;
        apply_directory_sql_migrations_postgres(&pool, migrations_dir.as_path()).await?;
        Ok(Self {
            pool,
            migrations_dir: Arc::new(migrations_dir),
        })
    }

    pub async fn migrate(&self) -> Result<()> {
        sqlx::migrate!("migrations/postgres")
            .run(&self.pool)
            .await?;
        migrate_catalog_postgres(&self.pool).await?;
        apply_directory_sql_migrations_postgres(&self.pool, self.migrations_dir.as_path()).await
    }

    async fn entity_field_types(&self, entity: &str) -> Result<HashMap<String, String>> {
        ensure_entity_name(entity)?;
        let row = sqlx::query("SELECT document FROM mb_entity_schema WHERE name = $1")
            .bind(entity)
            .fetch_optional(&self.pool)
            .await?;
        let Some(row) = row else {
            return Ok(HashMap::new());
        };
        let doc_s: String = row.try_get(0)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        Ok(super::catalog::field_types_from_document(&doc))
    }

    pub async fn verify_admin_basic(&self, email: &str, password: &str) -> Result<bool> {
        let row = sqlx::query("SELECT password_hash FROM mb_admin WHERE email = $1")
            .bind(email)
            .fetch_optional(&self.pool)
            .await?;
        let Some(row) = row else {
            return Ok(false);
        };
        let hash: String = row.try_get(0)?;
        let parsed =
            PasswordHash::new(&hash).map_err(|e| StorageError::Validation(e.to_string()))?;
        Ok(Argon2::default()
            .verify_password(password.as_bytes(), &parsed)
            .is_ok())
    }

    pub async fn list_admins(&self) -> Result<Vec<(String, String)>> {
        let rows = sqlx::query("SELECT id, email FROM mb_admin ORDER BY email")
            .fetch_all(&self.pool)
            .await?;
        let mut out = Vec::new();
        for row in rows {
            out.push((row.try_get(0)?, row.try_get(1)?));
        }
        Ok(out)
    }

    pub async fn add_admin(&self, email: &str, password: &str) -> Result<()> {
        let id = Uuid::new_v4().to_string();
        let salt = SaltString::generate(&mut OsRng);
        let hash = Argon2::default()
            .hash_password(password.as_bytes(), &salt)
            .map_err(|e| StorageError::Validation(e.to_string()))?
            .to_string();
        let ts = now_rfc3339();
        sqlx::query(
            r#"INSERT INTO mb_admin (id, email, password_hash, created_at, updated_at)
               VALUES ($1, $2, $3, $4, $5)"#,
        )
        .bind(&id)
        .bind(email)
        .bind(&hash)
        .bind(&ts)
        .bind(&ts)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    pub async fn remove_admin(&self, id_or_email: &str) -> Result<u64> {
        let r = sqlx::query("DELETE FROM mb_admin WHERE id = $1 OR email = $2")
            .bind(id_or_email)
            .bind(id_or_email)
            .execute(&self.pool)
            .await?;
        Ok(r.rows_affected())
    }

    pub async fn entity_type(&self, name: &str) -> Result<Option<String>> {
        ensure_entity_name(name)?;
        let row = sqlx::query("SELECT document FROM mb_entity_schema WHERE name = $1")
            .bind(name)
            .fetch_optional(&self.pool)
            .await?;
        let Some(row) = row else {
            return Ok(None);
        };
        let doc_s: String = row.try_get(0)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        Ok(doc
            .get("type")
            .and_then(|x| x.as_str())
            .map(std::string::ToString::to_string))
    }

    pub async fn list_entities_catalog(&self) -> Result<Vec<JsonValue>> {
        let rows = sqlx::query(
            "SELECT id, name, document FROM mb_entity_schema ORDER BY name",
        )
        .fetch_all(&self.pool)
        .await?;
        let mut out = Vec::new();
        for row in rows {
            let id: String = row.try_get(0)?;
            let name: String = row.try_get(1)?;
            let doc_s: String = row.try_get(2)?;
            let doc: JsonValue = serde_json::from_str(&doc_s)?;
            out.push(super::catalog::list_row_from_document(&id, &name, &doc));
        }
        Ok(out)
    }

    pub async fn get_entity_catalog(&self, name: &str) -> Result<Option<JsonValue>> {
        ensure_entity_name(name)?;
        let row = sqlx::query("SELECT document FROM mb_entity_schema WHERE name = $1")
            .bind(name)
            .fetch_optional(&self.pool)
            .await?;
        let Some(row) = row else {
            return Ok(None);
        };
        let doc_s: String = row.try_get(0)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        Ok(Some(super::catalog::full_catalog_json(name, &doc)))
    }

    pub async fn delete_entity_catalog(&self, name: &str) -> Result<()> {
        ensure_entity_name(name)?;
        let row = sqlx::query("SELECT document FROM mb_entity_schema WHERE name = $1")
            .bind(name)
            .fetch_optional(&self.pool)
            .await?;
        let Some(row) = row else {
            return Err(StorageError::NotFound(name.to_string()));
        };
        let doc_s: String = row.try_get(0)?;
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
        sqlx::query(&q).execute(&self.pool).await?;
        sqlx::query("DELETE FROM mb_entity_schema WHERE name = $1")
            .bind(name)
            .execute(&self.pool)
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
        let mut tx = self.pool.begin().await?;
        let entity_id = Uuid::new_v4().to_string();
        let ts = now_rfc3339();
        let doc = build_schema_document(&entity_id, &schema, view_sql, rules);
        let doc_s = serde_json::to_string(&doc)?;
        sqlx::query(
            r#"INSERT INTO mb_entity_schema (id, name, document, created_at, updated_at)
               VALUES ($1, $2, $3, $4, $5)"#,
        )
        .bind(&entity_id)
        .bind(name)
        .bind(&doc_s)
        .bind(&ts)
        .bind(&ts)
        .execute(&mut *tx)
        .await?;
        if !matches!(et, EntityType::View) {
            let ddl = build_create_table_ddl(name, &schema.fields)?;
            sqlx::query(&ddl).execute(&mut *tx).await?;
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
        tx.commit().await?;
        Ok(())
    }

    pub async fn patch_entity_catalog(
        &self,
        name: &str,
        patch: &super::EntityCatalogPatch,
    ) -> Result<()> {
        ensure_entity_name(name)?;
        let row = sqlx::query("SELECT id, document FROM mb_entity_schema WHERE name = $1")
            .bind(name)
            .fetch_optional(&self.pool)
            .await?;
        let Some(row) = row else {
            return Err(StorageError::NotFound(name.to_string()));
        };
        let entity_id: String = row.try_get(0)?;
        let doc_s: String = row.try_get(1)?;
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
        let mut tx = self.pool.begin().await?;
        let inner: Result<()> = async {
            for st in &stmts_postgres {
                let t = st.trim();
                if t.is_empty() || t.starts_with("--") {
                    continue;
                }
                sqlx::query(t).execute(&mut *tx).await?;
            }
            let ts = now_rfc3339();
            let new_doc_s = serde_json::to_string(&new_doc)?;
            sqlx::query(
                "UPDATE mb_entity_schema SET document = $1, updated_at = $2 WHERE name = $3",
            )
            .bind(&new_doc_s)
            .bind(&ts)
            .bind(name)
            .execute(&mut *tx)
            .await?;
            Ok(())
        }
        .await;
        match &inner {
            Ok(()) => {
                tx.commit().await?;
            }
            Err(_) => {
                tx.rollback().await?;
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
        let row = sqlx::query("SELECT document FROM mb_entity_schema WHERE name = $1")
            .bind(entity_name)
            .fetch_optional(&self.pool)
            .await?;
        let Some(row) = row else {
            return Ok(None);
        };
        let doc_s: String = row.try_get(0)?;
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
        let q = format!(
            r#"SELECT * FROM {} LIMIT $1 OFFSET $2"#,
            quote_ident(entity)?
        );
        let rows = sqlx::query(&q)
            .bind(limit as i64)
            .bind(offset as i64)
            .fetch_all(&self.pool)
            .await?;
        let mut out = Vec::new();
        for row in rows {
            out.push(pg_row_to_map(&row)?);
        }
        Ok(out)
    }

    pub async fn get_entity_row(
        &self,
        entity: &str,
        id: &str,
    ) -> Result<Option<Map<String, JsonValue>>> {
        ensure_entity_name(entity)?;
        let q = format!(r#"SELECT * FROM {} WHERE id = $1"#, quote_ident(entity)?);
        let row = sqlx::query(&q).bind(id).fetch_optional(&self.pool).await?;
        Ok(match row {
            Some(r) => Some(pg_row_to_map(&r)?),
            None => None,
        })
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
        let types = self.entity_field_types(entity).await?;
        let keys: Vec<String> = body.keys().cloned().collect();
        let quoted_table = quote_ident(entity)?;
        let mut qb: QueryBuilder<sqlx::Postgres> = QueryBuilder::new("INSERT INTO ");
        qb.push(quoted_table.as_str());
        qb.push(" (");
        {
            let mut sep = qb.separated(", ");
            for k in &keys {
                sep.push(quote_ident(k)?.as_str());
            }
        }
        qb.push(") VALUES (");
        for (i, k) in keys.iter().enumerate() {
            if i > 0 {
                qb.push(", ");
            }
            let ft = types.get(k).map(|s| s.as_str()).unwrap_or("string");
            push_json_bind_on_builder(&mut qb, ft, body.get(k).unwrap_or(&JsonValue::Null))?;
        }
        qb.push(")");
        let q = qb.build();
        q.execute(&self.pool).await?;
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
        let types = self.entity_field_types(entity).await?;
        let mut qb: QueryBuilder<sqlx::Postgres> = QueryBuilder::new("UPDATE ");
        qb.push(quote_ident(entity)?);
        qb.push(" SET ");
        let mut first = true;
        for (k, v) in &patch {
            if k == "created_at" {
                continue;
            }
            if !first {
                qb.push(", ");
            }
            first = false;
            let ft = types.get(k).map(|s| s.as_str()).unwrap_or("string");
            qb.push(format!("{} = ", quote_ident(k)?));
            push_json_bind_on_builder(&mut qb, ft, v)?;
        }
        if !first {
            qb.push(", ");
        }
        qb.push(format!("{} = ", quote_ident("updated_at")?));
        push_json_bind_on_builder(&mut qb, "string", &json!(now_rfc3339()))?;
        qb.push(" WHERE id = ");
        qb.push_bind(id);
        let q = qb.build();
        let n = q.execute(&self.pool).await?;
        if n.rows_affected() == 0 {
            return Err(StorageError::NotFound(id.to_string()));
        }
        self.get_entity_row(entity, id)
            .await?
            .ok_or_else(|| StorageError::NotFound(id.to_string()))
    }

    pub async fn delete_entity_row(&self, entity: &str, id: &str) -> Result<()> {
        ensure_entity_name(entity)?;
        let q = format!(r#"DELETE FROM {} WHERE id = $1"#, quote_ident(entity)?);
        let r = sqlx::query(&q).bind(id).execute(&self.pool).await?;
        if r.rows_affected() == 0 {
            return Err(StorageError::NotFound(id.to_string()));
        }
        Ok(())
    }

    pub async fn verify_user_login(
        &self,
        email: &str,
        password: &str,
    ) -> Result<Option<Map<String, JsonValue>>> {
        let row = sqlx::query(
            r#"SELECT id, email, profile_json, created_at, updated_at, password_hash
               FROM mb_user WHERE email = $1 LIMIT 1"#,
        )
        .bind(email)
        .fetch_optional(&self.pool)
        .await?;
        let Some(row) = row else {
            return Ok(None);
        };
        let hash: String = row.try_get("password_hash")?;
        let parsed =
            PasswordHash::new(&hash).map_err(|e| StorageError::Validation(e.to_string()))?;
        if Argon2::default()
            .verify_password(password.as_bytes(), &parsed)
            .is_err()
        {
            return Ok(None);
        }
        let id: String = row.try_get("id")?;
        let email_s: String = row.try_get("email")?;
        let profile: Option<String> = row.try_get("profile_json")?;
        let created_at: String = row.try_get("created_at")?;
        let updated_at: String = row.try_get("updated_at")?;
        let profile_val = match profile {
            Some(s) if !s.trim().is_empty() => serde_json::from_str(&s).unwrap_or(json!(s)),
            _ => JsonValue::Null,
        };
        let mut m = Map::new();
        m.insert("id".into(), json!(id));
        m.insert("email".into(), json!(email_s));
        m.insert("profile".into(), profile_val);
        m.insert("created_at".into(), json!(created_at));
        m.insert("updated_at".into(), json!(updated_at));
        Ok(Some(m))
    }

    pub async fn app_config_get(&self, key: &str) -> Result<Option<String>> {
        let row = sqlx::query("SELECT value FROM mb_app_config WHERE key = $1")
            .bind(key)
            .fetch_optional(&self.pool)
            .await?;
        Ok(row.map(|r| r.try_get(0)).transpose()?)
    }

    pub async fn app_config_set(
        &self,
        key: &str,
        value: &str,
        updated_by: Option<&str>,
    ) -> Result<()> {
        let ts = now_rfc3339();
        sqlx::query(
            r#"INSERT INTO mb_app_config (key, value, updated_at, updated_by)
               VALUES ($1, $2, $3, $4)
               ON CONFLICT (key) DO UPDATE SET
                 value = EXCLUDED.value,
                 updated_at = EXCLUDED.updated_at,
                 updated_by = EXCLUDED.updated_by"#,
        )
        .bind(key)
        .bind(value)
        .bind(&ts)
        .bind(updated_by)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    pub async fn app_config_list(&self) -> Result<Vec<(String, String)>> {
        let rows = sqlx::query("SELECT key, value FROM mb_app_config ORDER BY key")
            .fetch_all(&self.pool)
            .await?;
        let mut out = Vec::new();
        for row in rows {
            out.push((row.try_get(0)?, row.try_get(1)?));
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
        let ev = serde_json::to_string(events)?;
        let ts = now_rfc3339();
        sqlx::query(
            r#"INSERT INTO mb_webhook (id, url, secret_hmac, events, enabled, created_at)
               VALUES ($1, $2, $3, $4, 1, $5)"#,
        )
        .bind(&id)
        .bind(url)
        .bind(secret)
        .bind(&ev)
        .bind(&ts)
        .execute(&self.pool)
        .await?;
        Ok(id)
    }

    pub async fn list_webhooks(&self) -> Result<Vec<JsonValue>> {
        let rows = sqlx::query(
            "SELECT id, url, events, enabled, created_at FROM mb_webhook ORDER BY created_at",
        )
        .fetch_all(&self.pool)
        .await?;
        let mut out = Vec::new();
        for row in rows {
            out.push(json!({
                "id": row.try_get::<String, _>(0)?,
                "url": row.try_get::<String, _>(1)?,
                "events": serde_json::from_str::<JsonValue>(&row.try_get::<String, _>(2)?)?,
                "enabled": row.try_get::<i32, _>(3)? != 0,
                "created_at": row.try_get::<String, _>(4)?,
            }));
        }
        Ok(out)
    }
}

fn push_json_bind_on_builder(
    qb: &mut QueryBuilder<sqlx::Postgres>,
    ft: &str,
    v: &JsonValue,
) -> Result<()> {
    match ft {
        "int" => {
            let i: Option<i64> = match v {
                JsonValue::Null => None,
                JsonValue::Number(n) => n.as_i64(),
                JsonValue::String(s) => s.parse().ok(),
                _ => None,
            };
            qb.push_bind(i);
        }
        "bool" => {
            let b: Option<i32> = match v {
                JsonValue::Null => None,
                JsonValue::Bool(x) => Some(if *x { 1 } else { 0 }),
                JsonValue::Number(n) => n.as_i64().map(|x| if x != 0 { 1 } else { 0 }),
                JsonValue::String(s) => match s.to_lowercase().as_str() {
                    "true" | "1" => Some(1),
                    "false" | "0" => Some(0),
                    _ => None,
                },
                _ => None,
            };
            qb.push_bind(b);
        }
        "float" => {
            let f: Option<f64> = match v {
                JsonValue::Null => None,
                JsonValue::Number(n) => n.as_f64(),
                JsonValue::String(s) => s.parse().ok(),
                _ => None,
            };
            qb.push_bind(f);
        }
        _ => {
            let s: Option<String> = match v {
                JsonValue::Null => None,
                JsonValue::String(t) => Some(t.clone()),
                _ => Some(v.to_string()),
            };
            qb.push_bind(s);
        }
    }
    Ok(())
}

fn pg_row_to_map(row: &PgRow) -> Result<Map<String, JsonValue>> {
    let mut m = Map::new();
    for (i, col) in row.columns().iter().enumerate() {
        let name = col.name();
        let v = pg_cell_to_json(row, i)?;
        m.insert(name.to_string(), v);
    }
    Ok(m)
}

fn pg_cell_to_json(row: &PgRow, i: usize) -> Result<JsonValue> {
    if let Ok(v) = row.try_get::<Option<bool>, _>(i) {
        return Ok(json!(v));
    }
    if let Ok(v) = row.try_get::<Option<i64>, _>(i) {
        return Ok(json!(v));
    }
    if let Ok(v) = row.try_get::<Option<f64>, _>(i) {
        return Ok(json!(v));
    }
    if let Ok(v) = row.try_get::<Option<String>, _>(i) {
        let Some(s) = v else {
            return Ok(JsonValue::Null);
        };
        if (s.starts_with('{') || s.starts_with('['))
            && serde_json::from_str::<JsonValue>(&s).is_ok()
        {
            return Ok(serde_json::from_str(&s)?);
        }
        return Ok(JsonValue::String(s));
    }
    Ok(JsonValue::Null)
}
