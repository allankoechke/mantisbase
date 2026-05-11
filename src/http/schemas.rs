use std::sync::Arc;

use axum::extract::{Path, State};
use axum::http::{HeaderMap, StatusCode};
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};

use crate::models::types::{validate_entity_name, EntityType};
use crate::models::Field;

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

pub async fn list_schemas(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let v = state.store.list_entities_catalog().await?;
    Ok(Json(json!(v)))
}

pub async fn create_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<CreateSchemaBody>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
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
    let _ = require_admin(&headers, &state.store).await?;
    let v = state
        .store
        .get_entity_catalog(&name)
        .await?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "schema not found"))?;
    Ok(Json(v))
}

pub async fn delete_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(name): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    state.store.delete_entity_catalog(&name).await?;
    Ok(Json(json!({ "ok": true })))
}
