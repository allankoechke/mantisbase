use std::time::{SystemTime, UNIX_EPOCH};

use axum::http::{header, HeaderMap, StatusCode};
use base64::Engine;
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

#[derive(Debug, Clone)]
enum AuthorizationCredential {
    Basic { user: String, pass: String },
    Bearer(String),
}

fn scheme_prefix<'a>(raw: &'a str, scheme: &str) -> Option<&'a str> {
    let raw = raw.trim();
    if raw.len() < scheme.len() {
        return None;
    }
    if !raw[..scheme.len()].eq_ignore_ascii_case(scheme) {
        return None;
    }
    let rest = &raw[scheme.len()..];
    Some(rest.trim_start())
}

fn parse_authorization_header(raw: &str) -> Option<AuthorizationCredential> {
    let raw = raw.trim();
    if let Some(b64) = scheme_prefix(raw, "Basic") {
        let decoded = base64::engine::general_purpose::STANDARD.decode(b64).ok()?;
        let s = String::from_utf8(decoded).ok()?;
        let (user, pass) = s.split_once(':')?;
        return Some(AuthorizationCredential::Basic {
            user: user.to_string(),
            pass: pass.to_string(),
        });
    }
    if let Some(token) = scheme_prefix(raw, "Bearer") {
        if token.is_empty() {
            return None;
        }
        return Some(AuthorizationCredential::Bearer(token.to_string()));
    }
    // Some clients send the JWT alone without a "Bearer " prefix.
    if raw.starts_with("eyJ") && raw.contains('.') {
        return Some(AuthorizationCredential::Bearer(raw.to_string()));
    }
    None
}

fn parse_authorization(headers: &HeaderMap) -> Result<AuthorizationCredential, ApiError> {
    let raw = headers
        .get(header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .ok_or(ApiError::new(
            StatusCode::UNAUTHORIZED,
            "missing Authorization header",
        ))?;
    parse_authorization_header(raw).ok_or_else(|| {
        ApiError::new(
            StatusCode::UNAUTHORIZED,
            "Authorization must be Basic credentials or Bearer <token>",
        )
    })
}

async fn verify_admin_basic(
    state: &AppState,
    user: &str,
    pass: &str,
) -> Result<AdminPrincipal, ApiError> {
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
    Ok(AdminPrincipal {
        id: admin.id,
        email: admin.email,
    })
}

fn verify_admin_bearer(state: &AppState, token: &str) -> Result<AdminPrincipal, ApiError> {
    let token = token.trim();
    let mut validation = Validation::new(jsonwebtoken::Algorithm::HS256);
    validation.set_audience(&[MB_ADMIN_JWT_AUD]);
    let data = jsonwebtoken::decode::<AdminJwtClaims>(
        token,
        &DecodingKey::from_secret(state.signing_key.as_bytes()),
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
    Ok(AdminPrincipal {
        id,
        email: data.claims.email,
    })
}

async fn verify_admin_credential(
    state: &AppState,
    cred: &AuthorizationCredential,
) -> Result<AdminPrincipal, ApiError> {
    match cred {
        AuthorizationCredential::Basic { user, pass } => {
            verify_admin_basic(state, user, pass).await
        }
        AuthorizationCredential::Bearer(token) => verify_admin_bearer(state, token),
    }
}

fn verify_app_user_bearer(state: &AppState, token: &str) -> Result<(), ApiError> {
    let token = token.trim();
    let mut validation = Validation::new(jsonwebtoken::Algorithm::HS256);
    validation.validate_aud = false;
    let data = jsonwebtoken::decode::<AppUserClaims>(
        token,
        &DecodingKey::from_secret(state.signing_key.as_bytes()),
        &validation,
    )
    .map_err(|_| ApiError::new(StatusCode::UNAUTHORIZED, "invalid bearer token"))?;
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
        return Err(ApiError::new(
            StatusCode::UNAUTHORIZED,
            "invalid token subject",
        ));
    }
    Ok(())
}

/// Admin routes accept **HTTP Basic** (email:password) or **Bearer** JWT from [`super::admins::admin_auth_login`].
pub(in crate::http) async fn require_admin(
    headers: &HeaderMap,
    state: &AppState,
) -> Result<AdminPrincipal, ApiError> {
    let cred = parse_authorization(headers)?;
    verify_admin_credential(state, &cred).await
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
            let cred = match parse_authorization(headers) {
                Ok(c) => c,
                Err(e) => return Err(e),
            };
            if verify_admin_credential(state, &cred).await.is_ok() {
                return Ok(());
            }
            match cred {
                AuthorizationCredential::Bearer(token) => {
                    if verify_app_user_bearer(state, &token).is_ok() {
                        return Ok(());
                    }
                    // Admin JWTs also decode as app-user-shaped claims (extra `aud` is ignored).
                    if verify_admin_bearer(state, &token).is_ok() {
                        return Ok(());
                    }
                    Err(ApiError::new(
                        StatusCode::UNAUTHORIZED,
                        "invalid or expired bearer token",
                    ))
                }
                AuthorizationCredential::Basic { .. } => Err(ApiError::new(
                    StatusCode::UNAUTHORIZED,
                    "invalid admin credentials",
                )),
            }
        }
        AccessMode::Custom => Err(ApiError::new(
            StatusCode::FORBIDDEN,
            "custom access rules not implemented",
        )),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_bearer_case_insensitive() {
        let c = parse_authorization_header("bearer mytoken").unwrap();
        match c {
            AuthorizationCredential::Bearer(t) => assert_eq!(t, "mytoken"),
            _ => panic!("expected bearer"),
        }
    }

    #[test]
    fn parse_raw_jwt_without_scheme() {
        let jwt = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.sig";
        let c = parse_authorization_header(jwt).unwrap();
        match c {
            AuthorizationCredential::Bearer(t) => assert_eq!(t, jwt),
            _ => panic!("expected bearer"),
        }
    }
}
