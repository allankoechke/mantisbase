//! First-admin bootstrap: short-lived setup JWT, `/api/v1/setup/*`, and `/mb/setup` UI.

use std::sync::{Arc, RwLock};
use std::time::{SystemTime, UNIX_EPOCH};

use axum::extract::{Query, State};
use axum::http::{header, HeaderMap, StatusCode};
use axum::Json;
use jsonwebtoken::{DecodingKey, EncodingKey, Validation};
use rand::RngCore;
use serde::Deserialize;
use serde_json::{json, Value};
use uuid::Uuid;

use crate::logger::{cli_stdout_line, prelude::*};
use crate::models::{normalize_and_validate_admin_email, normalize_and_validate_admin_password};

use super::admins::admin_json;
use super::error::ApiError;
use super::jwt::{SetupJwtClaims, MB_SETUP_JWT_AUD};
use super::AppState;

pub const SETUP_TOKEN_TTL_SECS: u64 = 20 * 60;

#[derive(Debug, Default)]
struct ActiveSetup {
    jti: String,
    exp: u64,
    used: bool,
}

/// In-memory single-use setup token state (valid until first use, expiry, or an admin exists).
#[derive(Debug, Default)]
pub struct SetupState {
    inner: RwLock<Option<ActiveSetup>>,
}

impl SetupState {
    pub fn issue(&self, jti: String, exp: u64) {
        *self.inner.write().expect("setup lock") = Some(ActiveSetup {
            jti,
            exp,
            used: false,
        });
    }

    pub fn invalidate(&self) {
        *self.inner.write().expect("setup lock") = None;
    }

    pub fn token_active(&self, jti: &str, now: u64) -> bool {
        let g = self.inner.read().expect("setup lock");
        g.as_ref()
            .is_some_and(|a| !a.used && a.jti == jti && a.exp > now)
    }

    /// Returns `true` if `jti` matches the active token, is not expired/used, and is consumed.
    pub fn consume(&self, jti: &str, now: u64) -> bool {
        let mut g = self.inner.write().expect("setup lock");
        let Some(active) = g.as_mut() else {
            return false;
        };
        if active.used || active.jti != jti || active.exp <= now {
            return false;
        }
        active.used = true;
        true
    }
}

#[derive(Debug, Deserialize)]
pub struct SetupStatusQuery {
    pub token: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct FirstAdminBody {
    pub email: String,
    pub password: String,
    #[serde(default)]
    pub token: Option<String>,
}

pub fn signing_key_for_process(jwt_secret: Option<&str>) -> String {
    if let Some(s) = jwt_secret.filter(|s| !s.is_empty()) {
        return s.to_string();
    }
    let mut buf = [0u8; 32];
    rand::thread_rng().fill_bytes(&mut buf);
    hex::encode(buf)
}

pub fn encode_setup_token(signing_key: &str, jti: &str, exp: u64) -> Result<String, ApiError> {
    let claims = SetupJwtClaims {
        jti: jti.to_string(),
        exp,
        aud: MB_SETUP_JWT_AUD.to_string(),
    };
    jsonwebtoken::encode(
        &jsonwebtoken::Header::default(),
        &claims,
        &EncodingKey::from_secret(signing_key.as_bytes()),
    )
    .map_err(|_| ApiError::internal("setup token encode failed"))
}

fn decode_setup_token(signing_key: &str, token: &str) -> Result<SetupJwtClaims, ApiError> {
    let mut validation = Validation::new(jsonwebtoken::Algorithm::HS256);
    validation.set_audience(&[MB_SETUP_JWT_AUD]);
    let data = jsonwebtoken::decode::<SetupJwtClaims>(
        token.trim(),
        &DecodingKey::from_secret(signing_key.as_bytes()),
        &validation,
    )
    .map_err(|_| ApiError::new(StatusCode::UNAUTHORIZED, "invalid or expired setup token"))?;
    Ok(data.claims)
}

fn extract_setup_token(
    headers: &HeaderMap,
    query_or_body: Option<&str>,
) -> Result<String, ApiError> {
    if let Some(raw) = headers
        .get(header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
    {
        if let Some(t) = raw.strip_prefix("Bearer ") {
            if !t.trim().is_empty() {
                return Ok(t.trim().to_string());
            }
        }
    }
    if let Some(t) = query_or_body.filter(|s| !s.trim().is_empty()) {
        return Ok(t.trim().to_string());
    }
    Err(ApiError::new(
        StatusCode::UNAUTHORIZED,
        "setup token required (Bearer or token query/body field)",
    ))
}

async fn ensure_setup_allowed(state: &AppState) -> Result<(), ApiError> {
    if state.store.has_any_admin().await? {
        state.setup.invalidate();
        return Err(ApiError::new(
            StatusCode::FORBIDDEN,
            "setup is not available after an admin account exists",
        ));
    }
    Ok(())
}

pub async fn setup_status(
    State(state): State<Arc<AppState>>,
    Query(q): Query<SetupStatusQuery>,
) -> Result<Json<Value>, ApiError> {
    let needs_setup = !state.store.has_any_admin().await?;
    if !needs_setup {
        state.setup.invalidate();
        return Ok(Json(json!({
            "needs_setup": false,
            "token_valid": false,
        })));
    }
    let mut token_valid = false;
    if let Some(ref t) = q.token {
        if let Ok(claims) = decode_setup_token(&state.signing_key, t) {
            let now = unix_now()?;
            token_valid = state.setup.token_active(&claims.jti, now);
        }
    }
    Ok(Json(json!({
        "needs_setup": true,
        "token_valid": token_valid,
    })))
}

pub async fn create_first_admin(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<FirstAdminBody>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    ensure_setup_allowed(&state).await?;
    let email = normalize_and_validate_admin_email(&body.email).map_err(ApiError::bad_request)?;
    let password =
        normalize_and_validate_admin_password(&body.password).map_err(ApiError::bad_request)?;
    let token = extract_setup_token(&headers, body.token.as_deref())?;
    let claims = decode_setup_token(&state.signing_key, &token)?;
    let now = unix_now()?;
    if !state.setup.consume(&claims.jti, now) {
        return Err(ApiError::new(
            StatusCode::UNAUTHORIZED,
            "setup token is invalid, expired, or already used",
        ));
    }
    if state.store.has_any_admin().await? {
        state.setup.invalidate();
        return Err(ApiError::new(
            StatusCode::FORBIDDEN,
            "an admin account already exists, setup invalidated",
        ));
    }
    state.store.add_admin(&email, &password).await?;
    state.setup.invalidate();
    let admin = state
        .store
        .get_admin_by_email(&email)
        .await?
        .ok_or(ApiError::internal("admin missing after create"))?;
    Ok((
        StatusCode::CREATED,
        Json(json!({ "ok": true, "admin": admin_json(&admin) })),
    ))
}

fn unix_now() -> Result<u64, ApiError> {
    Ok(SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|_| ApiError::internal("clock error"))?
        .as_secs())
}

pub fn try_open_browser(url: &str) {
    let ok = if cfg!(target_os = "macos") {
        std::process::Command::new("open").arg(url).spawn().is_ok()
    } else if cfg!(target_os = "windows") {
        std::process::Command::new("cmd")
            .args(["/C", "start", "", url])
            .spawn()
            .is_ok()
    } else {
        ["xdg-open", "gio"]
            .iter()
            .any(|c| std::process::Command::new(c).arg(url).spawn().is_ok())
    };
    if !ok {
        warn!("could not launch a browser automatically, open the URL below in your browser");
    }
}

/// When no admins exist, mint a setup token and print the dashboard URL.
pub async fn bootstrap_first_admin_setup(
    host: &str,
    port: u16,
    state: &Arc<AppState>,
) -> anyhow::Result<()> {
    if state.skip_setup {
        info!("first-admin setup skipped (--skip-setup or MB_SKIP_ADMIN_SETUP)");
        return Ok(());
    }
    if state.store.has_any_admin().await? {
        return Ok(());
    }
    let jti = Uuid::new_v4().to_string();
    let exp = unix_now().map_err(|e| anyhow::anyhow!(e.1))? + SETUP_TOKEN_TTL_SECS;
    state.setup.issue(jti.clone(), exp);
    let token =
        encode_setup_token(&state.signing_key, &jti, exp).map_err(|e| anyhow::anyhow!(e.1))?;
    let setup_url = format!("http://{host}:{port}/mb/setup?token={token}");
    warn!(
        "no admin accounts found — complete setup within {} minutes",
        SETUP_TOKEN_TTL_SECS / 60
    );
    cli_stdout_line("\nFirst-time setup (no admin accounts):");
    cli_stdout_line(format!("  {setup_url}\n"));
    try_open_browser(&setup_url);
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn setup_token_is_single_use() {
        let s = SetupState::default();
        s.issue("jti-1".into(), 9_999_999_999);
        assert!(s.token_active("jti-1", 1));
        assert!(s.consume("jti-1", 1));
        assert!(!s.consume("jti-1", 2));
        assert!(!s.token_active("jti-1", 2));
    }
}
