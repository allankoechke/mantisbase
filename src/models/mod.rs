mod schema;
mod entity;

#[derive(Debug, Clone)]
pub enum AccessRule {
    Public,
    Admin,
    Authenticated,
    Custom
}
