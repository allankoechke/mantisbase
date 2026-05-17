use std::time::{SystemTime, UNIX_EPOCH};

use axum::http::{header, HeaderMap, StatusCode};
use jsonwebtoken::{DecodingKey, Validation};

use crate::models::types::{AccessMode, AccessRule};

use super::error::ApiError;
use super::jwt::{AdminJwtClaims, AppUserClaims, MB_ADMIN_JWT_AUD};
use super::AppState;

/// Authenticated admin identity (id + email).
#[derive(Debug, Clone)]
pub(in crate::http) struct AdminPrincipal {
    pub id: String,
    #[allow(dead_code)]
    pub email: String,
}

/// Admin routes accept **HTTP Basic** (email:password) or **Bearer** JWT from [`super::admins::admin_auth_login`].
pub(in crate::http) async fn require_admin(
    headers: &HeaderMap,
    state: &AppState,
) -> Result<AdminPrincipal, ApiError> {
    let raw = headers
        .get(header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .ok_or(ApiError::new(StatusCode::UNAUTHORIZED, "missing Authorization"))?;
    if let Some(b64) = raw.strip_prefix("Basic ") {
        let decoded = base64::Engine::decode(&base64::engine::general_purpose::STANDARD, b64)
            .map_err(|_| ApiError::new(StatusCode::UNAUTHORIZED, "invalid Basic encoding"))?;
        let s = String::from_utf8(decoded)
            .map_err(|_| ApiError::new(StatusCode::UNAUTHORIZED, "invalid Basic utf8"))?;
        let (user, pass) = s
            .split_once(':')
            .ok_or(ApiError::new(StatusCode::UNAUTHORIZED, "invalid Basic format"))?;
        let ok = state.store.verify_admin_basic(user, pass).await?;
        if !ok {
            return Err(ApiError::new(
                StatusCode::UNAUTHORIZED,
                "invalid admin credentials",
            ));
        }
        let admin = state
            .store
            .get_admin_by_email(user)
            .await?
            .ok_or(ApiError::new(
                StatusCode::UNAUTHORIZED,
                "invalid admin credentials",
            ))?;
        return Ok(AdminPrincipal {
            id: admin.id,
            email: admin.email,
        });
    }
    if let Some(token) = raw.strip_prefix("Bearer ") {
        let secret = state.jwt_secret.as_deref().ok_or(ApiError::new(
            StatusCode::INTERNAL_SERVER_ERROR,
            "MB_JWT_SECRET not set",
        ))?;
        let mut validation = Validation::new(jsonwebtoken::Algorithm::HS256);
        validation.set_audience(&[MB_ADMIN_JWT_AUD]);
        let data = jsonwebtoken::decode::<AdminJwtClaims>(
            token.trim(),
            &DecodingKey::from_secret(secret.as_bytes()),
            &validation,
        )
        .map_err(|_| {
            ApiError::new(
                StatusCode::UNAUTHORIZED,
                "invalid or non-admin bearer token",
            )
        })?;
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map_err(|_| ApiError::new(StatusCode::INTERNAL_SERVER_ERROR, "clock error"))?
            .as_secs();
        if data.claims.exp <= now {
            return Err(ApiError::new(StatusCode::UNAUTHORIZED, "token expired"));
        }
        let id = if data.claims.id.is_empty() {
            data.claims.sub
        } else {
            data.claims.id
        };
        if id.is_empty() || data.claims.email.is_empty() {
            return Err(ApiError::new(
                StatusCode::UNAUTHORIZED,
                "invalid admin token claims",
            ));
        }
        return Ok(AdminPrincipal {
            id,
            email: data.claims.email,
        });
    }
    Err(ApiError::new(
        StatusCode::UNAUTHORIZED,
        "expected Basic or Bearer admin authorization",
    ))
}

fn bearer_token(headers: &HeaderMap) -> Option<String> {
    let raw = headers.get(header::AUTHORIZATION)?.to_str().ok()?;
    raw.strip_prefix("Bearer ").map(|s| s.to_string())
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
            require_admin(headers, state).await?;
            Ok(())
        }
        AccessMode::Authenticated => {
            if require_admin(headers, state).await.is_ok() {
                return Ok(());
            }
            let token = bearer_token(headers).ok_or(ApiError::new(
                StatusCode::UNAUTHORIZED,
                "Bearer token or admin Basic required",
            ))?;
            let secret = state.jwt_secret.as_deref().ok_or(ApiError::new(
                StatusCode::INTERNAL_SERVER_ERROR,
                "MB_JWT_SECRET not set",
            ))?;
            let data = jsonwebtoken::decode::<AppUserClaims>(
                &token,
                &jsonwebtoken::DecodingKey::from_secret(secret.as_bytes()),
                &jsonwebtoken::Validation::default(),
            )
            .map_err(|_| ApiError::new(StatusCode::UNAUTHORIZED, "invalid token"))?;
            let now = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map_err(|_| ApiError::new(StatusCode::INTERNAL_SERVER_ERROR, "clock error"))?
                .as_secs();
            if data.claims.exp <= now {
                return Err(ApiError::new(StatusCode::UNAUTHORIZED, "token expired"));
            }
            let id = if data.claims.id.is_empty() {
                data.claims.sub.clone()
            } else {
                data.claims.id.clone()
            };
            if id.is_empty() {
                return Err(ApiError::new(StatusCode::UNAUTHORIZED, "invalid token subject"));
            }
            Ok(())
        }
        AccessMode::Custom => Err(ApiError::new(
            StatusCode::FORBIDDEN,
            "custom access rules not implemented",
        )),
    }
}
