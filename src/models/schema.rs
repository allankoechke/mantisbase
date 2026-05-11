use crate::models::types::{AccessRule, EntityType, Field, FieldType};

/// In-memory / API representation of an entity schema before or after persistence.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EntitySchema {
    pub entity_name: String,
    pub entity_type: EntityType,
    pub view_query: Option<String>,
    pub is_system: bool,
    pub has_api: bool,
    pub fields: Vec<Field>,
    pub list: AccessRule,
    pub read: AccessRule,
    pub create: AccessRule,
    pub update: AccessRule,
    pub delete: AccessRule,
}

impl EntitySchema {
    pub fn new(entity_name: String, entity_type: EntityType) -> Self {
        Self {
            entity_name,
            entity_type,
            view_query: None,
            is_system: false,
            has_api: true,
            fields: Vec::new(),
            list: AccessRule::admin_only(),
            read: AccessRule::admin_only(),
            create: AccessRule::admin_only(),
            update: AccessRule::admin_only(),
            delete: AccessRule::admin_only(),
        }
        .init()
    }

    fn init(mut self) -> Self {
        self.fields.clear();
        match self.entity_type {
            EntityType::Base => {
                self.fields.push(Field {
                    field_id: "id".to_string(),
                    field_name: "id".to_string(),
                    field_description: None,
                    field_type: FieldType::String,
                    is_required: true,
                    is_primary_key: true,
                    is_unique: true,
                    constraints: None,
                    default: None,
                });
                self.fields.push(Field {
                    field_id: "created_at".to_string(),
                    field_name: "created_at".to_string(),
                    field_description: None,
                    field_type: FieldType::DateTime,
                    is_required: true,
                    is_primary_key: false,
                    is_unique: false,
                    constraints: None,
                    default: None,
                });
                self.fields.push(Field {
                    field_id: "updated_at".to_string(),
                    field_name: "updated_at".to_string(),
                    field_description: None,
                    field_type: FieldType::DateTime,
                    is_required: true,
                    is_primary_key: false,
                    is_unique: false,
                    constraints: None,
                    default: None,
                });
            }
            EntityType::Bare | EntityType::View => {}
        }
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn base_entity_has_standard_fields() {
        let s = EntitySchema::new("t".to_string(), EntityType::Base);
        assert_eq!(s.fields.len(), 3);
        assert_eq!(s.fields[0].field_name, "id");
    }

    #[test]
    fn bare_entity_has_no_default_fields() {
        let s = EntitySchema::new("x".to_string(), EntityType::Bare);
        assert!(s.fields.is_empty());
    }
}
