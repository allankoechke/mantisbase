//! JWT claims for application users issued by [`super::login::auth_login`],
//! and admin operators issued by [`super::admins::admin_auth_login`].

use serde::{Deserialize, Serialize};

/// `aud` value for admin JWTs (must not match application-user tokens).
pub const MB_ADMIN_JWT_AUD: &str = "mantisbase_admin";

/// `aud` value for one-time first-admin setup tokens.
pub const MB_SETUP_JWT_AUD: &str = "mantisbase_setup";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SetupJwtClaims {
    pub jti: String,
    pub exp: u64,
    pub aud: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppUserClaims {
    /// User row id (same as [`Self::id`]).
    pub sub: String,
    pub id: String,
    pub email: String,
    pub exp: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AdminJwtClaims {
    /// Admin row id (same as [`Self::id`]).
    pub sub: String,
    pub id: String,
    pub email: String,
    pub exp: u64,
    pub aud: String,
}
