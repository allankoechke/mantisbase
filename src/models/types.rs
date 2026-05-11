//! Core catalog types shared by schema builders and persistence.

use serde::{Deserialize, Serialize};

/// Logical entity kind (matches REST `type` field).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum EntityType {
    Bare,
    Base,
    Auth,
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
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Field {
    pub field_id: String,
    pub field_name: String,
    pub field_description: Option<String>,
    pub field_type: FieldType,
    pub is_required: bool,
    pub is_primary_key: bool,
    pub is_unique: bool,
    pub constraints: Option<String>,
    pub default: Option<String>,
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
}
