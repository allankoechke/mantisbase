use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

use axum::extract::State;
use axum::http::StatusCode;
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};

use super::error::ApiError;
use super::AppState;

#[derive(Debug, Deserialize)]
pub struct AuthLoginBody {
    pub entity: String,
    pub identity: String,
    pub password: String,
}

#[derive(Debug, serde::Serialize, serde::Deserialize)]
struct JwtClaims {
    sub: String,
    entity: String,
    exp: u64,
}

pub async fn auth_login(
    State(state): State<Arc<AppState>>,
    Json(body): Json<AuthLoginBody>,
) -> Result<Json<Value>, ApiError> {
    let user = state
        .store
        .verify_auth_login(&body.entity, &body.identity, &body.password)
        .await?
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "invalid credentials"))?;
    let secret = state.jwt_secret.as_deref().ok_or(ApiError(
        StatusCode::INTERNAL_SERVER_ERROR,
        "MB_JWT_SECRET not set",
    ))?;
    let sub = user
        .get("id")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    let exp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs()
        + 86400 * 7;
    let claims = JwtClaims {
        sub,
        entity: body.entity.clone(),
        exp,
    };
    let token = jsonwebtoken::encode(
        &jsonwebtoken::Header::default(),
        &claims,
        &jsonwebtoken::EncodingKey::from_secret(secret.as_bytes()),
    )
    .map_err(|_| ApiError::internal("jwt encode failed"))?;
    Ok(Json(json!({ "token": token, "user": user })))
}
