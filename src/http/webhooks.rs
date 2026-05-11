use std::sync::Arc;

use axum::extract::State;
use axum::http::HeaderMap;
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};

use super::auth::require_admin;
use super::error::ApiError;
use super::AppState;

#[derive(Debug, Deserialize)]
pub struct RegisterHookBody {
    pub url: String,
    pub events: Vec<String>,
    #[serde(default)]
    pub secret: Option<String>,
}

pub async fn list_hooks(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let v = state.store.list_webhooks().await?;
    Ok(Json(json!(v)))
}

pub async fn register_hook(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<RegisterHookBody>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let id = state
        .store
        .register_webhook(&body.url, &body.events, body.secret.as_deref())
        .await?;
    Ok(Json(json!({ "id": id })))
}
