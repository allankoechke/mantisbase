//! Admin account HTTP API (`/api/v1/admins/*`), mirroring `mantisbase admins` CLI (HTTP Basic auth).

use std::sync::Arc;

use axum::extract::{Path, State};
use axum::http::{HeaderMap, StatusCode};
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};

use super::auth::require_admin;
use super::error::ApiError;
use super::AppState;

pub async fn list_admins(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let rows = state.store.list_admins().await?;
    let list: Vec<Value> = rows
        .into_iter()
        .map(|a| {
            json!({
                "id": a.id,
                "email": a.email,
                "active": a.active,
                "password_reset_required": a.password_reset_required,
            })
        })
        .collect();
    Ok(Json(json!({ "admins": list })))
}

#[derive(Debug, Deserialize)]
pub struct CreateAdminBody {
    pub email: String,
    pub password: String,
}

pub async fn create_admin(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<CreateAdminBody>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    state.store.add_admin(&body.email, &body.password).await?;
    Ok((StatusCode::CREATED, Json(json!({ "ok": true }))))
}

/// `id` is an admin row `id` or `email` (URL-encoded), same as CLI `admins --rm`.
pub async fn delete_admin(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(id): Path<String>,
) -> Result<StatusCode, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let n = state.store.remove_admin(&id).await?;
    if n == 0 {
        return Err(ApiError(StatusCode::NOT_FOUND, "admin not found"));
    }
    Ok(StatusCode::NO_CONTENT)
}
