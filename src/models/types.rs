//! Core catalog types shared by schema builders and persistence.

use std::collections::HashSet;

use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};

/// Logical entity kind (matches REST `type` field).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum EntityType {
    Bare,
    Base,
    View,
}

/// Physical / API field type.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum FieldType {
    String,
    Text,
    Password,
    Int,
    Float,
    Bool,
    DateTime,
    Json,
}

/// One field on an entity schema (API + DDL projection).
///
/// Stable identity is [`field_id`] (JSON `id`), assigned as `mbf_<sha256-prefix>` from [`field_name`]
/// via [`stable_field_id`]. [`field_name`] (JSON `name`) is the SQL column.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Field {
    #[serde(default, alias = "id")]
    pub field_id: String,
    #[serde(alias = "name")]
    pub field_name: String,
    #[serde(default, alias = "description")]
    pub field_description: Option<String>,
    #[serde(alias = "type")]
    pub field_type: FieldType,
    #[serde(default, alias = "required")]
    pub is_required: bool,
    #[serde(default, alias = "primary_key")]
    pub is_primary_key: bool,
    #[serde(default, alias = "unique")]
    pub is_unique: bool,
    #[serde(default)]
    pub constraints: Option<String>,
    #[serde(default)]
    pub default: Option<String>,
}

/// SHA-256 hex digest (first 128 bits) for reproducible catalog ids across installs.
pub fn stable_id_hex(input: &str) -> String {
    let digest = Sha256::digest(input.as_bytes());
    hex::encode(&digest[..16])
}

/// Catalog row id for an entity table: `mbt_<hash(name)>`.
pub fn stable_entity_id(entity_name: &str) -> String {
    format!("mbt_{}", stable_id_hex(entity_name))
}

/// Stable field id from SQL column name: `mbf_<hash(field_name)>`.
pub fn stable_field_id(field_name: &str) -> String {
    format!("mbf_{}", stable_id_hex(field_name))
}

/// Resolves `field_name`, assigns deterministic [`stable_field_id`], validates, and checks uniqueness.
pub fn normalize_fields(fields: &mut [Field]) -> Result<(), String> {
    for f in fields.iter_mut() {
        if f.field_name.is_empty() && !f.field_id.is_empty() {
            f.field_name = f.field_id.clone();
        }
        if f.field_name.is_empty() {
            return Err("each field must have a name".into());
        }
        validate_entity_name(&f.field_name).map_err(|m| format!("invalid field name: {m}"))?;
        if f.field_id.is_empty() {
            f.field_id = stable_field_id(&f.field_name);
        }
        validate_entity_name(&f.field_id).map_err(|m| format!("invalid field id: {m}"))?;
    }
    let mut ids = HashSet::new();
    let mut names = HashSet::new();
    for f in fields {
        if !ids.insert(f.field_id.clone()) {
            return Err(format!("duplicate field id `{}`", f.field_id));
        }
        if !names.insert(f.field_name.clone()) {
            return Err(format!("duplicate field name `{}`", f.field_name));
        }
    }
    Ok(())
}

/// Rule mode compatible with upstream JSON (`mode` + `expr`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum AccessMode {
    Public,
    Admin,
    /// Any logged-in user (JWT / session); upstream used `"auth"`.
    #[serde(alias = "auth")]
    Authenticated,
    Custom,
}

/// Access rule for one CRUD/list operation.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct AccessRule {
    pub mode: AccessMode,
    #[serde(default)]
    pub expr: String,
}

impl Default for AccessRule {
    fn default() -> Self {
        AccessRule::admin_only()
    }
}

impl AccessRule {
    pub fn admin_only() -> Self {
        Self {
            mode: AccessMode::Admin,
            expr: String::new(),
        }
    }

    pub fn public() -> Self {
        Self {
            mode: AccessMode::Public,
            expr: String::new(),
        }
    }

    pub fn mode_str(&self) -> &'static str {
        match self.mode {
            AccessMode::Public => "public",
            AccessMode::Admin => "admin",
            AccessMode::Authenticated => "authenticated",
            AccessMode::Custom => "custom",
        }
    }

    pub fn from_db(mode: &str, expr: Option<String>) -> Self {
        let mode = match mode {
            "public" => AccessMode::Public,
            "admin" => AccessMode::Admin,
            "authenticated" | "auth" => AccessMode::Authenticated,
            "custom" => AccessMode::Custom,
            _ => AccessMode::Admin,
        };
        Self {
            mode,
            expr: expr.unwrap_or_default(),
        }
    }
}

/// Validates entity names: alphanumeric + underscore, max 64 (SQL injection / identifier safety).
pub fn validate_entity_name(name: &str) -> Result<(), &'static str> {
    if name.is_empty() || name.len() > 64 {
        return Err("entity name length must be 1..=64");
    }
    if !name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
        return Err("entity name must be ASCII alphanumeric or underscore");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn validate_entity_name_ok() {
        assert!(validate_entity_name("posts").is_ok());
        assert!(validate_entity_name("user_1").is_ok());
    }

    #[test]
    fn validate_entity_name_rejects_bad() {
        assert!(validate_entity_name("").is_err());
        assert!(validate_entity_name("bad-name").is_err());
        assert!(validate_entity_name(&"a".repeat(65)).is_err());
    }

    #[test]
    fn access_rule_deserializes_auth_alias() {
        let j: AccessRule = serde_json::from_str(r#"{"mode":"auth","expr":""}"#).unwrap();
        assert_eq!(j.mode, AccessMode::Authenticated);
    }

    #[test]
    fn field_deserializes_name_as_id_default() {
        let f: Field =
            serde_json::from_str(r#"{"name":"title","type":"string","required":true}"#).unwrap();
        assert_eq!(f.field_name, "title");
        let mut fields = vec![f];
        normalize_fields(&mut fields).unwrap();
        assert_eq!(fields[0].field_id, stable_field_id("title"));
        assert!(fields[0].is_required);
    }

    #[test]
    fn stable_ids_are_deterministic() {
        assert_eq!(stable_entity_id("posts"), stable_entity_id("posts"));
        assert_eq!(stable_field_id("title"), stable_field_id("title"));
        assert!(stable_entity_id("posts").starts_with("mbt_"));
        assert!(stable_field_id("title").starts_with("mbf_"));
    }

    #[test]
    fn field_id_preserved_for_rename_patch() {
        let title_id = stable_field_id("title");
        let f: Field = serde_json::from_str(&format!(
            r#"{{"id":"{title_id}","name":"headline","type":"string","required":false}}"#
        ))
        .unwrap();
        let mut fields = vec![f];
        normalize_fields(&mut fields).unwrap();
        assert_eq!(fields[0].field_name, "headline");
        assert_eq!(fields[0].field_id, title_id);
    }
}
