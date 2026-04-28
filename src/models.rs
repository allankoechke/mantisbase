
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
pub struct BareModel {}

pub struct BaseEntityModel {
    entity_id: String,
    entity_name: String,
    entity_type: MantisBaseEntityType,
    description: Option<String>,
    is_system: bool,
    fields: Vec<Field>,
}

pub struct AuthModel {
    id: String,
    created_at: String,
    updated_at: String,
    email: String,
    password: String,
}