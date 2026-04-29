
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

impl EntitySchema {
    pub fn new(entity_name: String, entity_type: MantisBaseEntityType) -> Self {
        Self {
            entity_name,
            entity_type,
            view_query: None,
            is_system: false,
            has_api: true,
            fields: Vec::new(),
            list: AccessRule::Admin,
            read: AccessRule::Admin,
            create: AccessRule::Admin,
            update: AccessRule::Admin,
            delete: AccessRule::Admin,
        }.init()
    }

    fn init(mut self) -> Self {
        self.fields.clear();
        match self.entity_type {
            Base => {
                // Add `id`, `created` and `updated` fields
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
            },
            Auth => {
                // Add `id`, `created` and `updated` fields
                // Add `email`, `password`, etc
            },
            _ => self.fields.clear(),

        }
        self
    }
}
