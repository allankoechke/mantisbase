//! Authentication: HTTP Basic for admins today; pluggable providers for future OAuth/OIDC.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AuthContext {
    pub admin_email: Option<String>,
    pub user_entity: Option<String>,
    pub user_id: Option<String>,
}

/// Extensibility point for OAuth/OIDC (implemented in a later milestone).
pub trait AuthProvider: Send + Sync {
    fn id(&self) -> &'static str;
}

/// Built-in HTTP Basic against `mb_admin` (verified in HTTP middleware using [`crate::storage::Store`]).
#[derive(Debug, Default)]
pub struct BasicAdminAuth;

impl BasicAdminAuth {
    pub fn new() -> Self {
        Self
    }
}

#[derive(Debug, Default)]
pub struct OAuthPlaceholder;

impl AuthProvider for OAuthPlaceholder {
    fn id(&self) -> &'static str {
        "oauth_placeholder"
    }
}
