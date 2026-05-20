use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

use axum::extract::{Path, State};
use axum::http::{HeaderMap, StatusCode};
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};

use crate::models::{
    normalize_and_validate_admin_email, normalize_and_validate_admin_password, AdminRow,
};

use super::auth::require_admin;
use super::error::ApiError;
use super::jwt::{AdminJwtClaims, MB_ADMIN_JWT_AUD};
use super::AppState;

#[derive(Debug, Deserialize)]
pub struct AdminAuthLoginBody {
    pub email: String,
    pub password: String,
}

pub async fn admin_auth_login(
    State(state): State<Arc<AppState>>,
    Json(body): Json<AdminAuthLoginBody>,
) -> Result<Json<Value>, ApiError> {
    let email = normalize_and_validate_admin_email(&body.email).map_err(ApiError::bad_request)?;
    let password =
        normalize_and_validate_admin_password(&body.password).map_err(ApiError::bad_request)?;
    let Some(admin) = state.store.authenticate_admin(&email, &password).await? else {
        return Err(ApiError::new(
            StatusCode::UNAUTHORIZED,
            "invalid credentials",
        ));
    };
    let exp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs()
        + 86400 * 7;
    let claims = AdminJwtClaims {
        sub: admin.id.clone(),
        id: admin.id.clone(),
        email: admin.email.clone(),
        exp,
        aud: MB_ADMIN_JWT_AUD.to_string(),
    };
    let token = jsonwebtoken::encode(
        &jsonwebtoken::Header::default(),
        &claims,
        &jsonwebtoken::EncodingKey::from_secret(state.signing_key.as_bytes()),
    )
    .map_err(|_| ApiError::internal("jwt encode failed"))?;
    Ok(Json(json!({
        "token": token,
        "admin": admin_json(&admin),
    })))
}

pub(in crate::http) fn admin_json(a: &AdminRow) -> Value {
    json!({
        "id": a.id,
        "email": a.email,
        "active": a.active,
        "password_reset_required": a.password_reset_required,
    })
}

/// `id` is admin row id or email (URL-encoded), same as delete.
pub async fn get_admin(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(id): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state).await?;
    let admin = state
        .store
        .get_admin(&id)
        .await?
        .ok_or(ApiError::new(StatusCode::NOT_FOUND, "admin not found"))?;
    Ok(Json(admin_json(&admin)))
}

pub async fn list_admins(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state).await?;
    let rows = state.store.list_admins().await?;
    let list: Vec<Value> = rows.iter().map(admin_json).collect();
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
    let _ = require_admin(&headers, &state).await?;
    state.store.add_admin(&body.email, &body.password).await?;
    Ok((StatusCode::CREATED, Json(json!({}))))
}

/// `id` is an admin row `id` or `email` (URL-encoded), same as CLI `admins --rm`.
pub async fn delete_admin(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(id): Path<String>,
) -> Result<StatusCode, ApiError> {
    let _ = require_admin(&headers, &state).await?;
    let n = state.store.remove_admin(&id).await?;
    if n == 0 {
        return Err(ApiError::new(StatusCode::NOT_FOUND, "admin not found"));
    }
    Ok(StatusCode::NO_CONTENT)
}
