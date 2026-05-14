use std::sync::Arc;

use axum::extract::State;
use axum::Json;
use serde_json::{json, Value};

use super::error::ApiError;
use super::openapi::build_openapi_value;
use super::AppState;

pub async fn health() -> Json<Value> {
    Json(json!({ "status": "ok" }))
}

pub async fn openapi_json(State(state): State<Arc<AppState>>) -> Result<Json<Value>, ApiError> {
    let entities = state.store.list_entities_catalog().await?;
    Ok(Json(build_openapi_value(&entities)))
}
