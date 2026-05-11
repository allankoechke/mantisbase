use axum::http::{header, HeaderMap, StatusCode};

use crate::models::types::{AccessMode, AccessRule};
use crate::storage::Store;

use super::error::ApiError;
use super::AppState;

pub(in crate::http) async fn require_admin(
    headers: &HeaderMap,
    store: &Store,
) -> Result<String, ApiError> {
    let raw = headers
        .get(header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "missing Authorization"))?;
    let b64 = raw
        .strip_prefix("Basic ")
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "expected Basic auth"))?;
    let decoded = base64::Engine::decode(&base64::engine::general_purpose::STANDARD, b64)
        .map_err(|_| ApiError(StatusCode::UNAUTHORIZED, "invalid Basic encoding"))?;
    let s = String::from_utf8(decoded)
        .map_err(|_| ApiError(StatusCode::UNAUTHORIZED, "invalid Basic utf8"))?;
    let (user, pass) = s
        .split_once(':')
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "invalid Basic format"))?;
    let ok = store.verify_admin_basic(user, pass).await?;
    if !ok {
        return Err(ApiError(
            StatusCode::UNAUTHORIZED,
            "invalid admin credentials",
        ));
    }
    Ok(user.to_string())
}

fn bearer_token(headers: &HeaderMap) -> Option<String> {
    let raw = headers.get(header::AUTHORIZATION)?.to_str().ok()?;
    raw.strip_prefix("Bearer ").map(|s| s.to_string())
}

#[derive(Debug, serde::Serialize, serde::Deserialize)]
struct JwtClaims {
    sub: String,
    entity: String,
    exp: u64,
}

pub(in crate::http) async fn check_entity_access(
    state: &AppState,
    entity: &str,
    op: &str,
    headers: &HeaderMap,
) -> Result<(), ApiError> {
    let rule = state
        .store
        .get_access_rule(entity, op)
        .await?
        .unwrap_or_else(AccessRule::admin_only);
    match rule.mode {
        AccessMode::Public => Ok(()),
        AccessMode::Admin => {
            require_admin(headers, &state.store).await?;
            Ok(())
        }
        AccessMode::Authenticated => {
            if require_admin(headers, &state.store).await.is_ok() {
                return Ok(());
            }
            let token = bearer_token(headers).ok_or(ApiError(
                StatusCode::UNAUTHORIZED,
                "Bearer token or admin Basic required",
            ))?;
            let secret = state.jwt_secret.as_deref().ok_or(ApiError(
                StatusCode::INTERNAL_SERVER_ERROR,
                "MB_JWT_SECRET not set",
            ))?;
            let data = jsonwebtoken::decode::<JwtClaims>(
                &token,
                &jsonwebtoken::DecodingKey::from_secret(secret.as_bytes()),
                &jsonwebtoken::Validation::default(),
            )
            .map_err(|_| ApiError(StatusCode::UNAUTHORIZED, "invalid token"))?;
            if data.claims.entity != entity {
                return Err(ApiError(StatusCode::FORBIDDEN, "token entity mismatch"));
            }
            Ok(())
        }
        AccessMode::Custom => Err(ApiError(
            StatusCode::FORBIDDEN,
            "custom access rules not implemented",
        )),
    }
}
