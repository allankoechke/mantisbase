use std::sync::Arc;

use axum::extract::State;
use axum::http::HeaderMap;
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Map, Value};

use super::auth::require_admin;
use super::error::ApiError;
use super::AppState;

#[derive(Debug, Deserialize)]
pub struct PatchConfigBody {
    pub key: String,
    pub value: Value,
}

pub async fn list_config(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let rows = state.store.app_config_list().await?;
    let mut m = Map::new();
    for (k, v) in rows {
        let parsed: Value = serde_json::from_str(&v).unwrap_or_else(|_| json!(v));
        m.insert(k, parsed);
    }
    Ok(Json(Value::Object(m)))
}

pub async fn patch_config(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<PatchConfigBody>,
) -> Result<Json<Value>, ApiError> {
    let email = require_admin(&headers, &state.store).await?;
    let admins = state.store.list_admins().await?;
    let admin_id = admins
        .iter()
        .find(|(_, em)| *em == email)
        .map(|(id, _)| id.as_str());
    let v =
        serde_json::to_string(&body.value).map_err(|_| ApiError::bad_request("invalid JSON"))?;
    state.store.app_config_set(&body.key, &v, admin_id).await?;
    Ok(Json(json!({ "ok": true })))
}
