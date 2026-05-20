use std::sync::Arc;

use axum::extract::{Path, Query, State};
use axum::http::{HeaderMap, StatusCode};
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Map, Value};

use super::auth::check_entity_access;
use super::error::ApiError;
use super::AppState;

#[derive(Debug, Deserialize)]
pub struct ListQuery {
    #[serde(default = "default_limit")]
    pub limit: u32,
    #[serde(default)]
    pub offset: u32,
}

fn default_limit() -> u32 {
    50
}

pub async fn list_entities(
    State(state): State<Arc<AppState>>,
    Path(entity): Path<String>,
    Query(q): Query<ListQuery>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "list", &headers).await?;
    let rows = state
        .store
        .list_entity_rows(&entity, q.limit.min(500), q.offset)
        .await?;
    Ok(Json(json!(rows)))
}

pub async fn get_row(
    State(state): State<Arc<AppState>>,
    Path((entity, id)): Path<(String, String)>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "read", &headers).await?;
    let row = state
        .store
        .get_entity_row(&entity, &id)
        .await?
        .ok_or(ApiError::new(StatusCode::NOT_FOUND, "row not found"))?;
    Ok(Json(Value::Object(row.into_iter().collect())))
}

pub async fn create_row(
    State(state): State<Arc<AppState>>,
    Path(entity): Path<String>,
    headers: HeaderMap,
    Json(body): Json<Map<String, Value>>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    check_entity_access(&state, &entity, "create", &headers).await?;
    let row = state.store.insert_entity_row(&entity, body).await?;
    Ok((
        StatusCode::CREATED,
        Json(Value::Object(row.into_iter().collect())),
    ))
}

pub async fn update_row(
    State(state): State<Arc<AppState>>,
    Path((entity, id)): Path<(String, String)>,
    headers: HeaderMap,
    Json(patch): Json<Map<String, Value>>,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "update", &headers).await?;
    let row = state.store.update_entity_row(&entity, &id, patch).await?;
    Ok(Json(Value::Object(row.into_iter().collect())))
}

pub async fn delete_row(
    State(state): State<Arc<AppState>>,
    Path((entity, id)): Path<(String, String)>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "delete", &headers).await?;
    state.store.delete_entity_row(&entity, &id).await?;
    Ok(Json(json!({ "ok": true })))
}
