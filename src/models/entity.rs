
pub enum EntityType {
    Bare,
    Base,
    Auth,
    View
}

pub enum FieldType<T> {
    Text, // String type
    Password,
    Int(String), // u8, u16, u32, u64, i8, i16, i32, i64
    Float,
    Bool,
    TimeStamp, // Dat + Time + UTC offset
    Json,   // JSON object
    Array(T), // Vector array
}

pub struct EntityField {
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

trait EntityModel {
    fn new() -> Self;
    fn to_json(&self) -> String;
    fn from_json(&self, json: String) -> Self;
    fn to_sql(&self) -> String;


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