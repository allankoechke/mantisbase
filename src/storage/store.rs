//! Unified store handle (libSQL / Turso or PostgreSQL, selected at runtime).

use serde_json::{Map, Value};

use crate::models::types::{AccessRule, EntityType};
use crate::models::Field;

use super::error::Result;
use super::LibsqlStore;
use super::postgres::PostgresStore;

/// Active persistence backend for the HTTP server and CLI.
#[derive(Clone)]
pub enum Store {
    Libsql(LibsqlStore),
    Postgres(PostgresStore),
}

impl Store {
    pub async fn verify_admin_basic(&self, email: &str, password: &str) -> Result<bool> {
        match self {
            Store::Libsql(s) => s.verify_admin_basic(email, password).await,
            Store::Postgres(s) => s.verify_admin_basic(email, password).await,
        }
    }

    pub async fn entity_type(&self, name: &str) -> Result<Option<String>> {
        match self {
            Store::Libsql(s) => s.entity_type(name).await,
            Store::Postgres(s) => s.entity_type(name).await,
        }
    }

    pub async fn list_entities_catalog(&self) -> Result<Vec<Value>> {
        match self {
            Store::Libsql(s) => s.list_entities_catalog().await,
            Store::Postgres(s) => s.list_entities_catalog().await,
        }
    }

    pub async fn patch_entity_catalog(
        &self,
        name: &str,
        patch: &crate::storage::EntityCatalogPatch,
    ) -> Result<()> {
        match self {
            Store::Libsql(s) => s.patch_entity_catalog(name, patch).await,
            Store::Postgres(s) => s.patch_entity_catalog(name, patch).await,
        }
    }

    pub async fn delete_entity_catalog(&self, name: &str) -> Result<()> {
        match self {
            Store::Libsql(s) => s.delete_entity_catalog(name).await,
            Store::Postgres(s) => s.delete_entity_catalog(name).await,
        }
    }

    pub async fn create_entity_from_schema(
        &self,
        name: &str,
        et: EntityType,
        view_sql: Option<&str>,
        fields_override: Option<Vec<Field>>,
        rules: &Value,
    ) -> Result<()> {
        match self {
            Store::Libsql(s) => {
                s.create_entity_from_schema(name, et, view_sql, fields_override, rules)
                    .await
            }
            Store::Postgres(s) => {
                s.create_entity_from_schema(name, et, view_sql, fields_override, rules)
                    .await
            }
        }
    }

    pub async fn get_entity_catalog(&self, name: &str) -> Result<Option<Value>> {
        match self {
            Store::Libsql(s) => s.get_entity_catalog(name).await,
            Store::Postgres(s) => s.get_entity_catalog(name).await,
        }
    }

    pub async fn get_access_rule(&self, entity_name: &str, op: &str) -> Result<Option<AccessRule>> {
        match self {
            Store::Libsql(s) => s.get_access_rule(entity_name, op).await,
            Store::Postgres(s) => s.get_access_rule(entity_name, op).await,
        }
    }

    pub async fn list_entity_rows(
        &self,
        entity: &str,
        limit: u32,
        offset: u32,
    ) -> Result<Vec<Map<String, Value>>> {
        match self {
            Store::Libsql(s) => s.list_entity_rows(entity, limit, offset).await,
            Store::Postgres(s) => s.list_entity_rows(entity, limit, offset).await,
        }
    }

    pub async fn get_entity_row(
        &self,
        entity: &str,
        id: &str,
    ) -> Result<Option<Map<String, Value>>> {
        match self {
            Store::Libsql(s) => s.get_entity_row(entity, id).await,
            Store::Postgres(s) => s.get_entity_row(entity, id).await,
        }
    }

    pub async fn insert_entity_row(
        &self,
        entity: &str,
        body: Map<String, Value>,
    ) -> Result<Map<String, Value>> {
        match self {
            Store::Libsql(s) => s.insert_entity_row(entity, body).await,
            Store::Postgres(s) => s.insert_entity_row(entity, body).await,
        }
    }

    pub async fn update_entity_row(
        &self,
        entity: &str,
        id: &str,
        patch: Map<String, Value>,
    ) -> Result<Map<String, Value>> {
        match self {
            Store::Libsql(s) => s.update_entity_row(entity, id, patch).await,
            Store::Postgres(s) => s.update_entity_row(entity, id, patch).await,
        }
    }

    pub async fn delete_entity_row(&self, entity: &str, id: &str) -> Result<()> {
        match self {
            Store::Libsql(s) => s.delete_entity_row(entity, id).await,
            Store::Postgres(s) => s.delete_entity_row(entity, id).await,
        }
    }

    pub async fn verify_user_login(
        &self,
        email: &str,
        password: &str,
    ) -> Result<Option<Map<String, Value>>> {
        match self {
            Store::Libsql(s) => s.verify_user_login(email, password).await,
            Store::Postgres(s) => s.verify_user_login(email, password).await,
        }
    }

    pub async fn app_config_list(&self) -> Result<Vec<(String, String)>> {
        match self {
            Store::Libsql(s) => s.app_config_list().await,
            Store::Postgres(s) => s.app_config_list().await,
        }
    }

    pub async fn list_admins(&self) -> Result<Vec<(String, String)>> {
        match self {
            Store::Libsql(s) => s.list_admins().await,
            Store::Postgres(s) => s.list_admins().await,
        }
    }

    pub async fn app_config_set(
        &self,
        key: &str,
        value: &str,
        updated_by: Option<&str>,
    ) -> Result<()> {
        match self {
            Store::Libsql(s) => s.app_config_set(key, value, updated_by).await,
            Store::Postgres(s) => s.app_config_set(key, value, updated_by).await,
        }
    }

    pub async fn list_webhooks(&self) -> Result<Vec<Value>> {
        match self {
            Store::Libsql(s) => s.list_webhooks().await,
            Store::Postgres(s) => s.list_webhooks().await,
        }
    }

    pub async fn register_webhook(
        &self,
        url: &str,
        events: &[String],
        secret: Option<&str>,
    ) -> Result<String> {
        match self {
            Store::Libsql(s) => s.register_webhook(url, events, secret).await,
            Store::Postgres(s) => s.register_webhook(url, events, secret).await,
        }
    }

    pub async fn migrate(&self) -> Result<()> {
        match self {
            Store::Libsql(s) => s.migrate().await,
            Store::Postgres(s) => s.migrate().await,
        }
    }

    pub async fn add_admin(&self, email: &str, password: &str) -> Result<()> {
        match self {
            Store::Libsql(s) => s.add_admin(email, password).await,
            Store::Postgres(s) => s.add_admin(email, password).await,
        }
    }

    pub async fn remove_admin(&self, id_or_email: &str) -> Result<u64> {
        match self {
            Store::Libsql(s) => s.remove_admin(id_or_email).await,
            Store::Postgres(s) => s.remove_admin(id_or_email).await,
        }
    }
}
