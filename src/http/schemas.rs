use std::sync::Arc;

use axum::extract::{Path, State};
use axum::http::{HeaderMap, StatusCode};
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};

use crate::models::types::{validate_entity_name, EntityType};
use crate::models::Field;
use crate::storage::EntityCatalogPatch;

use super::auth::require_admin;
use super::error::ApiError;
use super::AppState;

#[derive(Debug, Deserialize)]
pub struct CreateSchemaBody {
    pub name: String,
    #[serde(rename = "type")]
    pub entity_type: EntityType,
    #[serde(default)]
    pub fields: Option<Vec<Field>>,
    #[serde(default)]
    pub view_sql: Option<String>,
    #[serde(default)]
    pub rules: Value,
}

#[derive(Debug, Deserialize)]
pub struct PatchSchemaBody {
    #[serde(rename = "type", default)]
    pub entity_type: Option<EntityType>,
    #[serde(default)]
    pub fields: Option<Vec<Field>>,
    /// JSON string or `null` to clear; omit key to leave unchanged.
    #[serde(default)]
    pub view_sql: Option<Value>,
    #[serde(default)]
    pub is_system: Option<bool>,
    #[serde(default)]
    pub has_api: Option<bool>,
    #[serde(default)]
    pub rules: Option<Value>,
    #[serde(default)]
    pub extra: Option<Value>,
}

impl PatchSchemaBody {
    fn is_noop(&self) -> bool {
        self.entity_type.is_none()
            && self.fields.is_none()
            && self.view_sql.is_none()
            && self.is_system.is_none()
            && self.has_api.is_none()
            && self.rules.is_none()
            && self.extra.is_none()
    }

    fn to_catalog_patch(&self) -> EntityCatalogPatch {
        EntityCatalogPatch {
            entity_type: self.entity_type,
            fields: self.fields.clone(),
            view_sql: self.view_sql.clone(),
            is_system: self.is_system,
            has_api: self.has_api,
            rules: self.rules.clone(),
            extra: self.extra.clone(),
        }
    }
}

pub async fn list_schemas(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state).await?;
    let v = state.store.list_entities_catalog().await?;
    Ok(Json(json!(v)))
}

pub async fn create_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<CreateSchemaBody>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    let _ = require_admin(&headers, &state).await?;
    validate_entity_name(&body.name).map_err(ApiError::bad_request)?;
    state
        .store
        .create_entity_from_schema(
            &body.name,
            body.entity_type,
            body.view_sql.as_deref(),
            body.fields,
            &body.rules,
        )
        .await?;
    Ok((
        StatusCode::CREATED,
        Json(json!({ "ok": true, "name": body.name })),
    ))
}

pub async fn get_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(name): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state).await?;
    let v = state
        .store
        .get_entity_catalog(&name)
        .await?
        .ok_or(ApiError::new(StatusCode::NOT_FOUND, "schema not found"))?;
    Ok(Json(v))
}

pub async fn patch_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(name): Path<String>,
    Json(body): Json<PatchSchemaBody>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state).await?;
    validate_entity_name(&name).map_err(ApiError::bad_request)?;
    if body.is_noop() {
        return Err(ApiError::bad_request("empty patch body"));
    }
    let patch = body.to_catalog_patch();
    state.store.patch_entity_catalog(&name, &patch).await?;
    let v = state
        .store
        .get_entity_catalog(&name)
        .await?
        .ok_or(ApiError::new(
            StatusCode::INTERNAL_SERVER_ERROR,
            "schema missing after patch",
        ))?;
    Ok(Json(v))
}

pub async fn delete_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(name): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state).await?;
    state.store.delete_entity_catalog(&name).await?;
    Ok(Json(json!({ "ok": true })))
}
