//! Strongly-typed projection for [`mb_user`](crate::storage) rows (no password hash).

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppUserRow {
    pub id: String,
    pub email: String,
    pub profile_json: Option<String>,
    pub created_at: String,
    pub updated_at: String,
}
