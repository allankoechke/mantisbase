//! JWT claims for application users issued by [`super::login::auth_login`],
//! and admin operators issued by [`super::admins::admin_auth_login`].

use serde::{Deserialize, Serialize};

/// `aud` value for admin JWTs (must not match application-user tokens).
pub const MB_ADMIN_JWT_AUD: &str = "mantisbase_admin";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppUserClaims {
    pub sub: String,
    pub exp: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AdminJwtClaims {
    pub sub: String,
    pub email: String,
    pub exp: u64,
    pub aud: String,
}
