
pub enum MantisBaseEntityType {
    Bare,
    Base,
    Auth,
    View
}

pub enum FieldType {
    String,
    Password,
    Int,
    Float,
    Bool,
    Date,
    DateTime,
    Json,
    Array,
}

pub struct Field {
    field_id: String,
    field_name: String,
    field_description: Option<String>,
    field_type: FieldType,
    is_required: bool,
    is_primary_key: bool,
    is_unique: bool,
    constraints: Option<String>,
    default: Option<String>,
}

pub enum AccessRule {
    Public,
    Admin,
    Authenticated,
    Custom
}

pub struct EntitySchema {
    entity_name: String,
    entity_type: MantisBaseEntityType,
    view_query: Option<String>,
    is_system: bool,
    has_api: bool,
    fields: Vec<Field>,
    list: AccessRule,
    read: AccessRule,
    create: AccessRule,
    update: AccessRule,
    delete: AccessRule,
}
pub struct BareModel {}

pub struct BaseEntityModel {
    entity_id: String,
    entity_name: String,
    entity_type: MantisBaseEntityType,
    description: Option<String>,
    is_system: bool,
    fields: Vec<Field>,
    foreign_keys: Vec<String>,
    constraints: Option<String>
}

pub struct AuthModel {
    id: String,
    created_at: String,
    updated_at: String,
    email: String,
    password: String,
}