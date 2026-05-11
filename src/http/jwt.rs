//! JWT claims for application users issued by [`super::login::auth_login`].

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppUserClaims {
    pub sub: String,
    pub exp: u64,
}
