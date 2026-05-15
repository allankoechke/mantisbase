//! Built-in administrator row (`mb_admin`).

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AdminRow {
    pub id: String,
    pub email: String,
    pub active: bool,
    pub password_reset_required: bool,
}
