//! Admin-only `/api/v1/sys/*` routes (schemas, configs, logs live under `sys` in [`super::mod`]).

use std::sync::Arc;

use axum::extract::{Query, State};
use axum::http::HeaderMap;
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};

use super::auth::require_admin;
use super::error::ApiError;
use super::AppState;

#[derive(Debug, Deserialize)]
pub struct LogsQuery {
    /// Max entries to return (capped); reserved for when a log sink is attached.
    #[serde(default)]
    pub limit: Option<u32>,
}

/// Returns log entries when a sink is integrated; currently an empty list (admin only).
pub async fn get_logs(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Query(q): Query<LogsQuery>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let limit = q.limit.unwrap_or(100).min(500);
    Ok(Json(json!({
        "entries": [],
        "limit": limit,
        "message": "Log tail over HTTP is not wired to a buffer yet; use process stdout or your host log stack."
    })))
}
