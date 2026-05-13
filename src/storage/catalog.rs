//! Single-table entity catalog (`mb_entity_schema`): one JSON document per entity (type, flags,
//! fields, rules). Legacy `mb_entity` / `mb_field` / `mb_access_rule` are migrated on boot.

use libsql::{params, Connection};
use serde_json::{json, Map, Value as JsonValue};

use crate::models::types::{AccessMode, AccessRule, EntityType, Field, FieldType};
use crate::models::EntitySchema;

use super::ddl::{build_create_table_ddl, ensure_entity_name};
use super::error::{Result, StorageError};

#[cfg(feature = "postgres")]
use sqlx::Row;

pub fn field_type_to_catalog_str(ft: &FieldType) -> &'static str {
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

fn catalog_str_to_field_type(s: &str) -> FieldType {
    match s {
        "string" => FieldType::String,
        "text" => FieldType::Text,
        "password" => FieldType::Password,
        "int" => FieldType::Int,
        "float" => FieldType::Float,
        "bool" => FieldType::Bool,
        "datetime" => FieldType::DateTime,
        "json" => FieldType::Json,
        _ => FieldType::String,
    }
}

pub fn parse_rule_from_json(rules: &JsonValue, op: &str) -> Option<AccessRule> {
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

/// Canonical JSON stored in `mb_entity_schema.document` (and returned by catalog APIs).
pub fn build_schema_document(
    entity_id: &str,
    schema: &EntitySchema,
    view_sql: Option<&str>,
    rules_input: &JsonValue,
) -> JsonValue {
    let mut rules_out = Map::new();
    for op in ["list", "read", "create", "update", "delete"] {
        let r = parse_rule_from_json(rules_input, op).unwrap_or_else(|| AccessRule::admin_only());
        rules_out.insert(
            op.to_string(),
            json!({ "mode": r.mode_str(), "expr": r.expr }),
        );
    }
    let fields: Vec<JsonValue> = schema
        .fields
        .iter()
        .map(|f| {
            json!({
                "field_id": f.field_id,
                "name": f.field_name,
                "type": field_type_to_catalog_str(&f.field_type),
                "description": f.field_description,
                "required": f.is_required,
                "primary_key": f.is_primary_key,
                "unique": f.is_unique,
                "constraints": f.constraints,
                "default": f.default,
            })
        })
        .collect();
    let type_str = match schema.entity_type {
        EntityType::Bare => "bare",
        EntityType::Base => "base",
        EntityType::View => "view",
    };
    json!({
        "version": 1,
        "entity_id": entity_id,
        "type": type_str,
        "view_sql": view_sql.or(schema.view_query.as_deref()),
        "is_system": schema.is_system,
        "has_api": schema.has_api,
        "fields": fields,
        "rules": rules_out,
        "extra": {},
    })
}

pub fn fields_from_document(doc: &JsonValue) -> Result<Vec<Field>> {
    let arr = doc
        .get("fields")
        .and_then(|x| x.as_array())
        .ok_or_else(|| StorageError::Validation("schema document missing fields array".into()))?;
    let mut out = Vec::new();
    for item in arr {
        let name = item
            .get("name")
            .and_then(|x| x.as_str())
            .ok_or_else(|| StorageError::Validation("field missing name".into()))?;
        let field_id = item
            .get("field_id")
            .and_then(|x| x.as_str())
            .unwrap_or(name)
            .to_string();
        let t = item
            .get("type")
            .and_then(|x| x.as_str())
            .unwrap_or("string");
        out.push(Field {
            field_id,
            field_name: name.to_string(),
            field_description: item
                .get("description")
                .and_then(|x| x.as_str())
                .map(String::from),
            field_type: catalog_str_to_field_type(t),
            is_required: item.get("required").and_then(|x| x.as_bool()).unwrap_or(false),
            is_primary_key: item
                .get("primary_key")
                .and_then(|x| x.as_bool())
                .unwrap_or(false),
            is_unique: item.get("unique").and_then(|x| x.as_bool()).unwrap_or(false),
            constraints: item
                .get("constraints")
                .and_then(|x| x.as_str())
                .map(String::from),
            default: item
                .get("default")
                .and_then(|x| x.as_str())
                .map(String::from),
        });
    }
    Ok(out)
}

pub fn access_rule_from_document(doc: &JsonValue, op: &str) -> Option<AccessRule> {
    let rules = doc.get("rules")?.as_object()?;
    let v = rules.get(op)?;
    let mode_raw = v.get("mode")?.as_str()?;
    let expr = v
        .get("expr")
        .and_then(|x| x.as_str())
        .unwrap_or("")
        .to_string();
    let mode = match mode_raw {
        "public" => AccessMode::Public,
        "admin" => AccessMode::Admin,
        "authenticated" | "auth" => AccessMode::Authenticated,
        "custom" => AccessMode::Custom,
        _ => AccessMode::Admin,
    };
    Some(AccessRule { mode, expr })
}

#[cfg(feature = "postgres")]
pub fn field_types_from_document(doc: &JsonValue) -> std::collections::HashMap<String, String> {
    let mut m = std::collections::HashMap::new();
    if let Some(arr) = doc.get("fields").and_then(|x| x.as_array()) {
        for item in arr {
            if let (Some(name), Some(t)) = (
                item.get("name").and_then(|x| x.as_str()),
                item.get("type").and_then(|x| x.as_str()),
            ) {
                m.insert(name.to_string(), t.to_string());
            }
        }
    }
    m
}

pub fn list_row_from_document(id: &str, name: &str, doc: &JsonValue) -> JsonValue {
    json!({
        "id": id,
        "name": name,
        "type": doc.get("type").and_then(|x| x.as_str()).unwrap_or("bare"),
        "view_sql": doc.get("view_sql"),
        "is_system": doc.get("is_system").and_then(|x| x.as_bool()).unwrap_or(false),
        "has_api": doc.get("has_api").and_then(|x| x.as_bool()).unwrap_or(true),
    })
}

pub fn full_catalog_json(name: &str, doc: &JsonValue) -> JsonValue {
    let rules = doc.get("rules").cloned().unwrap_or(json!({}));
    json!({
        "name": name,
        "type": doc.get("type").cloned().unwrap_or(json!("bare")),
        "view_sql": doc.get("view_sql").cloned(),
        "is_system": doc.get("is_system").cloned().unwrap_or(json!(false)),
        "has_api": doc.get("has_api").cloned().unwrap_or(json!(true)),
        "fields": doc.get("fields").cloned().unwrap_or(json!([])),
        "rules": rules,
        "extra": doc.get("extra").cloned().unwrap_or(json!({})),
    })
}

/// Partial update for [`LibsqlStore::patch_entity_catalog`] / [`PostgresStore::patch_entity_catalog`].
#[derive(Debug, Clone, Default)]
pub struct EntityCatalogPatch {
    pub entity_type: Option<EntityType>,
    pub fields: Option<Vec<Field>>,
    pub view_sql: Option<JsonValue>,
    pub is_system: Option<bool>,
    pub has_api: Option<bool>,
    pub rules: Option<JsonValue>,
    pub extra: Option<JsonValue>,
}

pub fn entity_schema_from_document(entity_name: &str, doc: &JsonValue) -> Result<EntitySchema> {
    let entity_type = match doc.get("type").and_then(|x| x.as_str()).unwrap_or("bare") {
        "bare" => EntityType::Bare,
        "base" => EntityType::Base,
        "view" => EntityType::View,
        _ => EntityType::Bare,
    };
    Ok(EntitySchema {
        entity_name: entity_name.to_string(),
        entity_type,
        view_query: doc
            .get("view_sql")
            .and_then(|v| v.as_str())
            .map(std::string::ToString::to_string),
        is_system: doc.get("is_system").and_then(|x| x.as_bool()).unwrap_or(false),
        has_api: doc.get("has_api").and_then(|x| x.as_bool()).unwrap_or(true),
        fields: fields_from_document(doc)?,
        list: access_rule_from_document(doc, "list").unwrap_or_else(|| AccessRule::admin_only()),
        read: access_rule_from_document(doc, "read").unwrap_or_else(|| AccessRule::admin_only()),
        create: access_rule_from_document(doc, "create").unwrap_or_else(|| AccessRule::admin_only()),
        update: access_rule_from_document(doc, "update").unwrap_or_else(|| AccessRule::admin_only()),
        delete: access_rule_from_document(doc, "delete").unwrap_or_else(|| AccessRule::admin_only()),
    })
}

pub fn merge_rules_input_for_patch(doc: &JsonValue, rules_patch: Option<&JsonValue>) -> JsonValue {
    match rules_patch {
        None => json!({ "rules": doc.get("rules").cloned().unwrap_or(json!({})) }),
        Some(pr) => merge_rules_document(doc, pr),
    }
}

fn merge_rules_document(doc: &JsonValue, patch_rules: &JsonValue) -> JsonValue {
    let mut base = doc
        .get("rules")
        .and_then(|x| x.as_object())
        .cloned()
        .unwrap_or_default();
    let patch_obj = patch_rules
        .get("rules")
        .and_then(|x| x.as_object())
        .or_else(|| patch_rules.as_object());
    if let Some(po) = patch_obj {
        for (k, v) in po {
            base.insert(k.clone(), v.clone());
        }
    }
    json!({ "rules": JsonValue::Object(base) })
}

pub fn apply_entity_catalog_patch(
    schema: &mut EntitySchema,
    patch: &EntityCatalogPatch,
) -> Result<()> {
    if let Some(et) = patch.entity_type {
        schema.entity_type = et;
    }
    if let Some(fields) = &patch.fields {
        schema.fields = fields.clone();
    }
    if let Some(v) = &patch.view_sql {
        match v {
            JsonValue::Null => schema.view_query = None,
            JsonValue::String(s) => {
                schema.view_query = if s.is_empty() {
                    None
                } else {
                    Some(s.clone())
                };
            }
            _ => {
                return Err(StorageError::Validation(
                    "view_sql must be a JSON string or null".into(),
                ));
            }
        }
    }
    if let Some(s) = patch.is_system {
        schema.is_system = s;
    }
    if let Some(h) = patch.has_api {
        schema.has_api = h;
    }
    if schema.entity_type == EntityType::View {
        let vs = schema.view_query.as_deref().unwrap_or("").trim();
        if vs.is_empty() {
            return Err(StorageError::Validation(
                "view entity requires non-empty view_sql".into(),
            ));
        }
    }
    Ok(())
}

pub fn preserve_extra_on_document(
    new_doc: &mut JsonValue,
    old_doc: &JsonValue,
    patch: &EntityCatalogPatch,
) {
    match &patch.extra {
        Some(e) => {
            new_doc["extra"] = e.clone();
        }
        None => {
            new_doc["extra"] = old_doc.get("extra").cloned().unwrap_or(json!({}));
        }
    }
}

async fn libsql_table_exists(conn: &Connection, table: &str) -> Result<bool> {
    let mut rows = conn
        .query(
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1",
            params![table],
        )
        .await?;
    Ok(rows.next().await?.is_some())
}

async fn libsql_scalar_i64(conn: &Connection, sql: &str) -> Result<i64> {
    let mut rows = conn.query(sql, ()).await?;
    let Some(row) = rows.next().await? else {
        return Ok(0);
    };
    Ok(row.get::<i64>(0)?)
}

pub async fn reconcile_physical_tables_libsql(conn: &Connection) -> Result<()> {
    let mut rows = conn
        .query(
            "SELECT name, document FROM mb_entity_schema ORDER BY name",
            (),
        )
        .await?;
    while let Some(row) = rows.next().await? {
        let name: String = row.get(0)?;
        let doc_s: String = row.get(1)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        let ty = doc
            .get("type")
            .and_then(|x| x.as_str())
            .unwrap_or("bare");
        if ty == "view" {
            continue;
        }
        ensure_entity_name(&name)?;
        let fields = fields_from_document(&doc)?;
        let ddl = build_create_table_ddl(&name, &fields)?;
        conn.execute(&ddl, ()).await?;
    }
    Ok(())
}

async fn drop_legacy_catalog_libsql(conn: &Connection) -> Result<()> {
    if libsql_table_exists(conn, "mb_access_rule").await? {
        conn.execute("DROP TABLE IF EXISTS mb_access_rule", ())
            .await?;
    }
    if libsql_table_exists(conn, "mb_field").await? {
        conn.execute("DROP TABLE IF EXISTS mb_field", ()).await?;
    }
    if libsql_table_exists(conn, "mb_entity").await? {
        conn.execute("DROP TABLE IF EXISTS mb_entity", ()).await?;
    }
    Ok(())
}

pub async fn migrate_catalog_libsql(conn: &Connection) -> Result<()> {
    if !libsql_table_exists(conn, "mb_entity_schema").await? {
        return Ok(());
    }
    let legacy = libsql_table_exists(conn, "mb_entity").await?;
    if !legacy {
        reconcile_physical_tables_libsql(conn).await?;
        return Ok(());
    }
    let n_unified = libsql_scalar_i64(conn, "SELECT COUNT(*) FROM mb_entity_schema").await?;
    if n_unified > 0 {
        drop_legacy_catalog_libsql(conn).await?;
        reconcile_physical_tables_libsql(conn).await?;
        return Ok(());
    }
    let mut erows = conn
        .query(
            "SELECT id, name, type, view_sql, is_system, has_api, created_at, updated_at FROM mb_entity",
            (),
        )
        .await?;
    while let Some(row) = erows.next().await? {
        let eid: String = row.get(0)?;
        let ename: String = row.get(1)?;
        let etype: String = row.get(2)?;
        let view_sql: Option<String> = row.get(3)?;
        let is_system: i64 = row.get(4)?;
        let has_api: i64 = row.get(5)?;
        let created_at: String = row.get(6)?;
        let updated_at: String = row.get(7)?;
        let mut fields = Vec::new();
        let mut frows = conn
            .query(
                "SELECT name, field_type, description, is_required, is_primary_key, is_unique, constraints_json, default_expr FROM mb_field WHERE entity_id = ? ORDER BY name",
                params![eid.clone()],
            )
            .await?;
        while let Some(fr) = frows.next().await? {
            let fname: String = fr.get(0)?;
            let ft: String = fr.get(1)?;
            fields.push(json!({
                "field_id": fname.clone(),
                "name": fname,
                "type": ft,
                "description": fr.get::<Option<String>>(2)?,
                "required": fr.get::<i64>(3)? != 0,
                "primary_key": fr.get::<i64>(4)? != 0,
                "unique": fr.get::<i64>(5)? != 0,
                "constraints": fr.get::<Option<String>>(6)?,
                "default": fr.get::<Option<String>>(7)?,
            }));
        }
        let mut rules = Map::new();
        let mut rrows = conn
            .query(
                "SELECT op, mode, expr FROM mb_access_rule WHERE entity_id = ?",
                params![eid.clone()],
            )
            .await?;
        while let Some(rr) = rrows.next().await? {
            let op: String = rr.get(0)?;
            let mode: String = rr.get(1)?;
            let expr: Option<String> = rr.get(2)?;
            rules.insert(
                op,
                json!({ "mode": mode, "expr": expr.unwrap_or_default() }),
            );
        }
        let doc = json!({
            "version": 1,
            "entity_id": eid,
            "type": etype,
            "view_sql": view_sql,
            "is_system": is_system != 0,
            "has_api": has_api != 0,
            "fields": fields,
            "rules": rules,
            "extra": {},
        });
        let doc_s = serde_json::to_string(&doc)?;
        conn.execute(
            "INSERT INTO mb_entity_schema (id, name, document, created_at, updated_at) VALUES (?, ?, ?, ?, ?)",
            params![eid.clone(), ename, doc_s, created_at, updated_at],
        )
        .await?;
    }
    drop_legacy_catalog_libsql(conn).await?;
    reconcile_physical_tables_libsql(conn).await?;
    Ok(())
}

#[cfg(feature = "postgres")]
pub async fn reconcile_physical_tables_postgres(pool: &sqlx::PgPool) -> Result<()> {
    let rows = sqlx::query("SELECT name, document FROM mb_entity_schema ORDER BY name")
        .fetch_all(pool)
        .await?;
    for row in rows {
        let name: String = row.try_get(0)?;
        let doc_s: String = row.try_get(1)?;
        let doc: JsonValue = serde_json::from_str(&doc_s)?;
        let ty = doc
            .get("type")
            .and_then(|x| x.as_str())
            .unwrap_or("bare");
        if ty == "view" {
            continue;
        }
        ensure_entity_name(&name)?;
        let fields = fields_from_document(&doc)?;
        let ddl = build_create_table_ddl(&name, &fields)?;
        sqlx::query(&ddl).execute(pool).await?;
    }
    Ok(())
}

#[cfg(feature = "postgres")]
async fn pg_table_exists(pool: &sqlx::PgPool, table: &str) -> Result<bool> {
    let r = sqlx::query_scalar::<_, bool>(
        "SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = 'public' AND table_name = $1)",
    )
    .bind(table)
    .fetch_one(pool)
    .await?;
    Ok(r)
}

#[cfg(feature = "postgres")]
async fn drop_legacy_catalog_postgres(pool: &sqlx::PgPool) -> Result<()> {
    if pg_table_exists(pool, "mb_access_rule").await? {
        sqlx::query("DROP TABLE IF EXISTS mb_access_rule CASCADE")
            .execute(pool)
            .await?;
    }
    if pg_table_exists(pool, "mb_field").await? {
        sqlx::query("DROP TABLE IF EXISTS mb_field CASCADE")
            .execute(pool)
            .await?;
    }
    if pg_table_exists(pool, "mb_entity").await? {
        sqlx::query("DROP TABLE IF EXISTS mb_entity CASCADE")
            .execute(pool)
            .await?;
    }
    Ok(())
}

#[cfg(feature = "postgres")]
pub async fn migrate_catalog_postgres(pool: &sqlx::PgPool) -> Result<()> {
    if !pg_table_exists(pool, "mb_entity_schema").await? {
        return Ok(());
    }
    if !pg_table_exists(pool, "mb_entity").await? {
        reconcile_physical_tables_postgres(pool).await?;
        return Ok(());
    }
    let n_unified: i64 =
        sqlx::query_scalar("SELECT COUNT(*)::bigint FROM mb_entity_schema")
            .fetch_one(pool)
            .await?;
    if n_unified > 0 {
        drop_legacy_catalog_postgres(pool).await?;
        reconcile_physical_tables_postgres(pool).await?;
        return Ok(());
    }
    let erows = sqlx::query(
        "SELECT id, name, type, view_sql, is_system, has_api, created_at, updated_at FROM mb_entity",
    )
    .fetch_all(pool)
    .await?;
    for row in erows {
        let eid: String = row.try_get(0)?;
        let ename: String = row.try_get(1)?;
        let etype: String = row.try_get(2)?;
        let view_sql: Option<String> = row.try_get(3)?;
        let is_system: i32 = row.try_get(4)?;
        let has_api: i32 = row.try_get(5)?;
        let created_at: String = row.try_get(6)?;
        let updated_at: String = row.try_get(7)?;
        let fields_rows = sqlx::query(
            r#"SELECT name, field_type, description, is_required, is_primary_key, is_unique,
                      constraints_json, default_expr FROM mb_field
               WHERE entity_id = $1 ORDER BY name"#,
        )
        .bind(&eid)
        .fetch_all(pool)
        .await?;
        let mut fields = Vec::new();
        for fr in fields_rows {
            let fname: String = fr.try_get(0)?;
            let ft: String = fr.try_get(1)?;
            fields.push(json!({
                "field_id": fname.clone(),
                "name": fname,
                "type": ft,
                "description": fr.try_get::<Option<String>, _>(2)?,
                "required": fr.try_get::<i32, _>(3)? != 0,
                "primary_key": fr.try_get::<i32, _>(4)? != 0,
                "unique": fr.try_get::<i32, _>(5)? != 0,
                "constraints": fr.try_get::<Option<String>, _>(6)?,
                "default": fr.try_get::<Option<String>, _>(7)?,
            }));
        }
        let rules_rows =
            sqlx::query("SELECT op, mode, expr FROM mb_access_rule WHERE entity_id = $1")
                .bind(&eid)
                .fetch_all(pool)
                .await?;
        let mut rules = Map::new();
        for rr in rules_rows {
            let op: String = rr.try_get(0)?;
            let mode: String = rr.try_get(1)?;
            let expr: Option<String> = rr.try_get(2)?;
            rules.insert(
                op,
                json!({ "mode": mode, "expr": expr.unwrap_or_default() }),
            );
        }
        let doc = json!({
            "version": 1,
            "entity_id": eid,
            "type": etype,
            "view_sql": view_sql,
            "is_system": is_system != 0,
            "has_api": has_api != 0,
            "fields": fields,
            "rules": rules,
            "extra": {},
        });
        let doc_s = serde_json::to_string(&doc)?;
        sqlx::query(
            r#"INSERT INTO mb_entity_schema (id, name, document, created_at, updated_at)
               VALUES ($1, $2, $3, $4, $5)"#,
        )
        .bind(&eid)
        .bind(&ename)
        .bind(&doc_s)
        .bind(&created_at)
        .bind(&updated_at)
        .execute(pool)
        .await?;
    }
    drop_legacy_catalog_postgres(pool).await?;
    reconcile_physical_tables_postgres(pool).await?;
    Ok(())
}

