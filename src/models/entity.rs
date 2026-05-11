//! Optional strongly-typed projections for auth rows (runtime rows are JSON maps).

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AuthUserRow {
    pub id: String,
    pub created_at: String,
    pub updated_at: String,
    pub email: String,
}
