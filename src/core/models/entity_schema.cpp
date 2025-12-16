#include "../../../include/mantisbase/core/models/entity_schema.h"

#include "mantisbase/core/exceptions.h"

namespace mb {
    EntitySchema::EntitySchema(const std::string &entity_name, const std::string &entity_type) {
        // Ensure name is valid
        if (!EntitySchema::isValidEntityName(entity_name)) {
            throw MantisException(400, "Invalid entity name, expected alphanumeric + _ only!", entity_name);
        }

        if (!EntitySchema::isValidEntityType(entity_type)) {
            throw MantisException(400, "Invalid entity type, expected `base`, `auth` or `view` only!", entity_type);
        }

        setName(entity_name).setType(entity_type);
    }

    EntitySchema::EntitySchema(const EntitySchema &other) = default;

    EntitySchema &EntitySchema::operator=(const EntitySchema &other) {
        if (this != &other) {
            m_name = other.m_name;
            m_type = other.m_type;
            m_viewSqlQuery = other.m_viewSqlQuery;
            m_isSystem = other.m_isSystem;
            m_hasApi = other.m_hasApi;
            m_viewSqlQuery = other.m_viewSqlQuery;
            m_fields = other.m_fields;
            m_listRule = other.m_listRule;
            m_getRule = other.m_getRule;
            m_addRule = other.m_addRule;
            m_updateRule = other.m_updateRule;
            m_deleteRule = other.m_deleteRule;
        }
        return *this;
    }

    EntitySchema::~EntitySchema() = default;

    bool EntitySchema::operator==(const EntitySchema &other) const {
        return toJSON() == other.toJSON();
    }

    EntitySchema EntitySchema::fromSchema(const json &entity_schema) {
        EntitySchema eSchema;

        if (!entity_schema.contains("name") || !entity_schema.contains("type"))
            throw MantisException(400, "Missing required fields `name` and `type` in schema!");

        const auto _name = entity_schema.at("name").get<std::string>();
        const auto _type = entity_schema.at("type").get<std::string>();

        // Ensure name is valid
        if (!EntitySchema::isValidEntityName(_name)) {
            throw MantisException(400, "Invalid entity name, expected alphanumeric + _ only!", _name);
        }

        if (!EntitySchema::isValidEntityType(_type)) {
            throw MantisException(400, "Invalid entity type, expected `base`, `auth` or `view` only!", _type);
        }

        eSchema.setName(_name);
        eSchema.setType(_type);

        if (entity_schema.contains("system") && entity_schema["system"].is_boolean())
            eSchema.setSystem(entity_schema.at("system").get<bool>());

        if (entity_schema.contains("has_api") && entity_schema["has_api"].is_boolean())
            eSchema.setHasApi(entity_schema.at("has_api").get<bool>());

        if (entity_schema.contains("rules")) {
            auto rules = entity_schema["rules"];

            if (rules.contains("list"))
                eSchema.setListRule(AccessRule::fromJSON(rules["list"]));

            if (rules.contains("get"))
                eSchema.setGetRule(AccessRule::fromJSON(rules["get"]));

            if (rules.contains("add"))
                eSchema.setAddRule(AccessRule::fromJSON(rules["add"]));

            if (rules.contains("update"))
                eSchema.setUpdateRule(AccessRule::fromJSON(rules["update"]));

            if (rules.contains("delete"))
                eSchema.setDeleteRule(AccessRule::fromJSON(rules["delete"]));
        }

        // 'auth' and 'base' types have 'fields' key ...
        if ((_type == "base" || _type == "auth") && entity_schema.contains("fields") && entity_schema["fields"].
            is_array()) {
            // For each defined field, create the field schema and push to the fields array ...
            for (const auto &field: entity_schema["fields"]) {
                // logger::trace("Field\n\t- {}", field.dump());
                auto field_name = field["name"].get<std::string>();
                if (!eSchema.hasField(field_name)) {
                    // logger::trace("Adding entity field: {}", field.dump());
                    eSchema.addField(EntitySchemaField(field));
                } else {
                    eSchema.field(field_name).updateWith(field);
                }
            }
        }

        // For 'view' types, check for 'view_query'
        if (_type == "view" && entity_schema.contains("view_query") && entity_schema["view_query"].is_string()
            && !entity_schema.at("view_query").get<std::string>().empty()) {
            eSchema.setViewQuery(entity_schema.at("view_query").get<std::string>());
        }

        return eSchema;
    }

    EntitySchema EntitySchema::fromEntity(const Entity &entity) {
        EntitySchema eSchema;

        // Default schema fields
        eSchema.setName(entity.name());
        eSchema.setType(entity.type());
        eSchema.setHasApi(entity.hasApi());
        eSchema.setSystem(entity.isSystem());

        // Reset fields to clear default added fields
        eSchema.fields().clear();

        // Rules
        eSchema.setAddRule(entity.addRule());
        eSchema.setGetRule(entity.getRule());
        eSchema.setListRule(entity.listRule());
        eSchema.setUpdateRule(entity.updateRule());
        eSchema.setDeleteRule(entity.deleteRule());

        // Fields ...
        if (entity.type() != "view") {
            eSchema.fields().reserve(entity.fields().size());
            for (const auto &field: entity.fields()) {
                eSchema.addField(EntitySchemaField(field));
            }
        }

        // For 'view' types, check for 'view_query'
        if (entity.type() == "view" && !entity.viewQuery().empty()) {
            eSchema.setViewQuery(entity.viewQuery());
        }

        return eSchema;
    }

    Entity EntitySchema::toEntity() const {
        return Entity{toJSON()};
    }

    std::string EntitySchema::id() const {
        if (m_name.empty()) throw MantisException(400, "Expected table name is empty!");
        return EntitySchema::genEntityId(m_name);
    }

    EntitySchemaField &EntitySchema::field(const std::string &field_name) {
        if (field_name.empty()) throw MantisException(500, "Empty field name provided.");

        const auto it = std::ranges::find_if(m_fields, [field_name](const auto &field) {
            return field_name == field.name();
        });

        if (it == m_fields.end())
            throw MantisException(404, "Field not found for name `" + field_name + "`");

        return *it;
    }

    EntitySchemaField &EntitySchema::fieldById(const std::string &field_id) {
        if (field_id.empty()) throw std::invalid_argument("Empty field id.");

        const auto it = std::ranges::find_if(m_fields, [field_id](const auto &field) {
            return field_id == field.id();
        });

        if (it == m_fields.end())
            throw std::out_of_range("Field not found for id `" + field_id + "`");

        return *it;
    }

    std::string EntitySchema::viewQuery() const {
        return m_viewSqlQuery;
    }

    EntitySchema &EntitySchema::setViewQuery(const std::string &viewQuery) {
        if (viewQuery.empty()) throw std::invalid_argument("Empty view query statement.");
        // TODO check if its a valid SQL query?
        m_viewSqlQuery = viewQuery;
        return *this;
    }

    void EntitySchema::updateWith(const nlohmann::json &new_data) {
        if (new_data.empty()) return;

        if (new_data.contains("name")) {
            if (!new_data["name"].is_string() || new_data["name"].empty()) {
                throw MantisException(400, "Expected name to be a valid string.");
            }

            setName(new_data["name"].get<std::string>());
        }

        if (new_data.contains("type")) {
            if (!new_data["type"].is_string() || new_data["type"].empty()) {
                throw MantisException(400, "Expected `type` to be a valid string.");
            }

            setType(new_data["type"].get<std::string>());
        }

        if (new_data.contains("system")) {
            if (!new_data["system"].is_boolean()) {
                throw MantisException(400, "Expected `system` to be a bool.");
            }

            setSystem(new_data.at("system").get<bool>());
        }

        if (new_data.contains("has_api")) {
            if (!new_data["has_api"].is_boolean()) {
                throw MantisException(400, "Expected `has_api` to be a bool.");
            }

            setHasApi(new_data.at("has_api").get<bool>());
    }

        if (new_data.contains("rules")) {
            auto rules = new_data["rules"];

            if (rules.contains("list")) {
                setListRule(AccessRule::fromJSON(rules["list"]));
            }

            if (rules.contains("get"))
                setGetRule(AccessRule::fromJSON(rules["get"]));

            if (rules.contains("add"))
                setAddRule(AccessRule::fromJSON(rules["add"]));

            if (rules.contains("update"))
                setUpdateRule(AccessRule::fromJSON(rules["update"]));

            if (rules.contains("delete"))
                setDeleteRule(AccessRule::fromJSON(rules["delete"]));
        }

        // 'auth' and 'base' types have 'fields' key ...
        if ((m_type == "base" || m_type == "auth") && new_data.contains("fields") && new_data["fields"].
            is_array()) {
            for (const auto &field: new_data["fields"]) {
                const auto name = field.contains("name") && field["name"].is_string()
                                      ? field["name"].get<std::string>()
                                      : "";
                const auto id = field.contains("id") && field["id"].is_string()
                                    ? field["id"].get<std::string>()
                                    : "";

                if (name.empty() && id.empty())
                    throw MantisException(
                        400, "At least field `id` or `name` should be provided for each field entry.");

                if (!id.empty() && hasFieldById(id)) {
                    // Update existing item by id
                    auto &f = fieldById(id);

                    // Check if op is set to delete/remove
                    if (field.contains("op") && field["op"].is_string() && !field["op"].empty()) {
                        const auto op = field["op"].get<std::string>();
                        if (op == "delete" || op == "remove") {
                            removeField(f.name());
                            continue;
                        }

                        throw MantisException(400, "Field `op` expected `remove` or `delete` value but found `" + op + "`");
                    }

                    f.updateWith(field);
                } else if (!name.empty() && hasField(name)) {
                    // try updating via name
                    auto &f = EntitySchema::field(name);

                    // Check if op is set to delete/remove
                    if (field.contains("op") && field["op"].is_string() && !field["op"].empty()) {
                        const auto op = field["op"].get<std::string>();
                        if (op == "delete" || op == "remove") {
                            removeField(f.name());
                            continue;
                        }
                    }

                    f.updateWith(field);
                } else {
                    // create new field
                    m_fields.emplace_back(field);
                }
            }
        }

        // For 'view' types, check for 'view_query'
        if (m_type == "view" && new_data.contains("view_query") && new_data["view_query"].is_string()
            && !new_data.at("view_query").get<std::string>().empty()) {
            setViewQuery(new_data.at("view_query").get<std::string>());
        }
    }

    std::string EntitySchema::name() const { return m_name; }

    EntitySchema &EntitySchema::setName(const std::string &name) {
        m_name = name;
        return *this;
    }

    std::string EntitySchema::type() const { return m_type; }

    EntitySchema &EntitySchema::setType(const std::string &type) {
        if (!(type == "base" || type == "auth" || type == "view"))
            throw std::invalid_argument("Type should either be `base`, `auth` or `view` only.");

        if (type == "view")
            m_fields.clear(); // Clear all fields if we are turning it to a view type

        else if (type == "base") {
            // Add base fields if they don't exist yet ...
            addFieldsIfNotExist("base");
        } else {
            // Add auth specific system fields ...
            addFieldsIfNotExist("auth");
        }

        m_type = type;
        return *this;
    }

    bool EntitySchema::hasApi() const { return m_hasApi; }

    EntitySchema &EntitySchema::setHasApi(const bool &hasApi) {
        m_hasApi = hasApi;
        return *this;
    }

    bool EntitySchema::isSystem() const { return m_isSystem; }

    EntitySchema &EntitySchema::setSystem(const bool &isSystem) {
        m_isSystem = isSystem;
        return *this;
    }

    AccessRule EntitySchema::listRule() const { return m_listRule; }

    EntitySchema &EntitySchema::setListRule(const AccessRule &listRule) {
        m_listRule = listRule;
        return *this;
    }

    AccessRule EntitySchema::getRule() const { return m_getRule; }

    EntitySchema &EntitySchema::setGetRule(const AccessRule &getRule) {
        m_getRule = getRule;
        return *this;
    }

    AccessRule EntitySchema::addRule() const { return m_addRule; }

    EntitySchema &EntitySchema::setAddRule(const AccessRule &addRule) {
        m_addRule = addRule;
        return *this;
    }

    AccessRule EntitySchema::updateRule() const { return m_updateRule; }

    EntitySchema &EntitySchema::setUpdateRule(const AccessRule &updateRule) {
        m_updateRule = updateRule;
        return *this;
    }

    AccessRule EntitySchema::deleteRule() const { return m_deleteRule; }

    EntitySchema &EntitySchema::setDeleteRule(const AccessRule &deleteRule) {
        m_deleteRule = deleteRule;
        return *this;
    }

    std::vector<EntitySchemaField> EntitySchema::fields() const { return m_fields; }

    EntitySchema &EntitySchema::addField(const EntitySchemaField &field) {
        if (const auto err = field.validate(); err.has_value()) {
            throw MantisException(400, std::format("Field validation failed for entity schema with message: {}",
                                                   err.value()));
        }

        m_fields.push_back(field);
        return *this;
    }

    bool EntitySchema::removeField(const std::string &field_name) {
        // If the field does not exist yet, return false
        if (!hasField(field_name)) return false;

        std::erase_if(
            m_fields,
            [&](const EntitySchemaField &field) { return field.name() == field_name; }
        );

        return true;
    }

    nlohmann::json EntitySchema::toJSON() const {
        json j;
        j["id"] = id();
        j["name"] = m_name;
        j["type"] = m_type;
        j["system"] = m_isSystem;
        j["has_api"] = m_hasApi;

        j["rules"] = {
            {"list", m_listRule.toJSON()},
            {"get", m_getRule.toJSON()},
            {"add", m_addRule.toJSON()},
            {"update", m_updateRule.toJSON()},
            {"delete", m_deleteRule.toJSON()}
        };

        if (m_type == "view") {
            j["view_query"] = m_viewSqlQuery;
        } else {
            j["fields"] = json::array();
            for (const auto &field: m_fields) {
                j["fields"].emplace_back(field.toJSON());
            }
        }
        return j;
    }

    std::string EntitySchema::toDDL() const {
        const auto sql = MantisBase::instance().db().session();

        std::ostringstream ddl;
        ddl << "CREATE TABLE IF NOT EXISTS " << m_name << " (";
        for (size_t i = 0; i < m_fields.size(); ++i) {
            if (i > 0) ddl << ", ";
            const auto field = m_fields[i];

            ddl << field.name()
                    << " "
                    << getFieldType(field.type(), sql);

            if (field.isPrimaryKey()) ddl << " PRIMARY KEY";
            if (field.required()) ddl << " NOT NULL";
            if (field.isUnique()) ddl << " UNIQUE";
            if (field.constraints().contains("default_value") && !field.constraints()["default_value"].is_null())
                ddl << " DEFAULT " << toDefaultSqlValue(field.type(), field.constraints()["default_value"]);
        }
        ddl << ");";

        return ddl.str();
    }

    bool EntitySchema::hasField(const std::string &field_name) const {
        if (m_type == "view") return false;

        return std::ranges::find_if(m_fields, [field_name](const EntitySchemaField &field) {
            return field.name() == field_name;
        }) != m_fields.end();
    }

    bool EntitySchema::hasFieldById(const std::string &field_id) const {
        if (field_id.empty()) return false;
        if (m_type == "view") return false;

        return std::ranges::find_if(m_fields, [field_id](const EntitySchemaField &field) {
            return field.id() == field_id;
        }) != m_fields.end();
    }

    std::string EntitySchema::toDefaultSqlValue(const std::string &type, const nlohmann::json &v) {
        if (type.empty())
            throw std::invalid_argument("Required field type can't be empty!");

        if (v.is_null()) return "NULL";

        if (type == "xml" || type == "string") {
            return "'" + v.dump() + "'";
        }

        if (type == "double" || type == "int8" || type == "uint8"
            || type == "int16" || type == "uint16" || type == "int32"
            || type == "uint32" || type == "int64" || type == "uint64"
            || type == "date" || type == "json" || type == "blob"
            || type == "date" || type == "file" || type == "files") {
            return v.dump();
        }

        if (type == "bool") {
            return v.get<bool>() ? "1" : "0";
        }

        throw std::runtime_error("Unsupported field type `" + type + "`");
    }

    std::string EntitySchema::dump() const {
        auto str = std::format(
            "\n\tid: {}\n\tName: {}\n\tType: {}\n\tIs System? {}\n\tHas API? {}\n\tRules:\n\t\t- list: {}\n\t\t- get: {}\n\t\t- add: {}\n\t\t- update: {}\n\t\t- delete: {}",
            id(),
            m_name,
            m_type,
            m_isSystem,
            m_hasApi,
            m_listRule.toJSON().dump(),
            m_getRule.toJSON().dump(),
            m_addRule.toJSON().dump(),
            m_updateRule.toJSON().dump(),
            m_deleteRule.toJSON().dump()
        );

        if (m_type == "view") {
            str += std::format("\n\tView Query: `{}`", m_viewSqlQuery);
        } else {
            str += "\n\tFields:";
            for (const auto &field: m_fields) {
                str += std::format("\n\t  - Name: `{}`\n\t\tSchema: {}", field.name(), field.toJSON().dump());
            }
        }

        // logger::trace("Entity Schema {}", str);

        return str;
    }

    std::string EntitySchema::genEntityId(const std::string &entity_name) {
        return "mbt_" + std::to_string(std::hash<std::string>{}(entity_name));
    }

    std::optional<std::string> EntitySchema::validate(const EntitySchema &table_schema) {
        if (table_schema.name().empty())
            return "Entity schema name is empty!";

        if (!(table_schema.type() == "base" || table_schema.type() == "view" || table_schema.type() == "auth")) {
            return "Expected entity type to be either `base`, `auth` or `view`!";
        }

        if (table_schema.type() == "view") {
            if (trim(table_schema.viewQuery()).empty())
                return "Entity schema view query is empty!";
            // TODO check if query is a valid SQL view query
        } else {
            // First validate each field
            for (const auto &field: table_schema.fields()) {
                if (auto err = field.validate(); err.has_value())
                    return err.value();
            }

            // Check that base fields are present
            if (table_schema.type() == "base") {
                for (const auto &field_name: EntitySchemaField::defaultBaseFields()) {
                    if (!table_schema.hasField(field_name))
                        return "Entity schema does not have field: `" + field_name + "` required for `" + table_schema.
                               type() + "` types.";
                }
            }

            if (table_schema.type() == "auth") {
                for (const auto &field_name: EntitySchemaField::defaultAuthFields()) {
                    if (!table_schema.hasField(field_name))
                        return "Entity schema does not have field: `" + field_name + "` required for `" + table_schema.
                               type() + "` types.";
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> EntitySchema::validate() const {
        return validate(*this);
    }

    bool EntitySchema::isValidEntityType(const std::string &type) {
        if (type == "base" || type == "view" || type == "auth") {
            return true;
        }
        return false;
    }

    bool EntitySchema::isValidEntityName(const std::string &name) {
        if (name.empty() || name.length() > 64) return false;
        return std::ranges::all_of(name, [](const char c) {
            return std::isalnum(c) || c == '_';
        });
    }

    std::string EntitySchema::getFieldType(const std::string &type, std::shared_ptr<soci::session> sql) {
        const auto db_type = sql->get_backend()->get_backend_name();
        // For date types, enforce `text` type SQLite ONLY
        if (db_type == "sqlite3" && type == "date") {
            return "text";
        }

        // PostgreSQL has no support for `db_uint` and `db_uint8` so lets store them in `db_uint16`/`db_int16 types`
        if (db_type == "postgresql" && (type == "uint8" || type == "int8")) {
            return type == "uint8"
                       ? sql->get_backend()->create_column_type(soci::db_uint16, 0, 0)
                       : sql->get_backend()->create_column_type(soci::db_int16, 0, 0);
        }

        // Convert bool types to `db_uint16`
        if (db_type == "postgresql" && type == "bool") {
            return sql->get_backend()->create_column_type(soci::db_uint16, 0, 0);
        }

        // General catch for all other types
        return sql->get_backend()->create_column_type(EntitySchemaField::toSociType(type), 0, 0);
    }

    void EntitySchema::addFieldsIfNotExist(const std::string &type) {
        if (type == "base") {
            for (const auto &field: EntitySchema::defaultBaseFieldsSchema()) {
                if (!hasField(field.name())) {
                    addField(field);
                }
            }
        } else if (type == "auth") {
            for (const auto &field: EntitySchema::defaultAuthFieldsSchema()) {
                if (!hasField(field.name())) {
                    addField(field);
                }
            }
        } else {
            throw MantisException(500, "Operation not supported for `view` types.");
        }
    }
} // mb
