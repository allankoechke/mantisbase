#include "../../../include/mantisbase/core/models/entity_schema_field.h"
#include "../../../include/mantisbase/utils/utils.h"
#include "../../../include/mantisbase/core/exceptions.h"
#include "../../../include/mantisbase/utils/soci_wrappers.h"

namespace mb {
    EntitySchemaField::EntitySchemaField(std::string field_name, std::string field_type)
        : m_name(std::move(field_name)),
          m_type(std::move(field_type)),
          m_constraints(defaultConstraints()) {
    }

    EntitySchemaField::EntitySchemaField(const nlohmann::json &field_schema)
        : m_constraints(defaultConstraints()) {
        if (!field_schema.contains("name") || !field_schema["name"].is_string() || field_schema["name"].empty())
            throw MantisException(400, "Field name is required!");

        if (!field_schema.contains("type") || !field_schema["type"].is_string() || field_schema["type"].empty())
            throw MantisException(400, "Field type is required!");

        updateWith(field_schema);
    }

    EntitySchemaField::EntitySchemaField(const EntitySchemaField &other) = default;

    bool EntitySchemaField::operator==(const EntitySchemaField &other) const {
        return toJSON() == other.toJSON();
    }

    EntitySchemaField &EntitySchemaField::updateWith(const nlohmann::json &field_schema) {
        if (field_schema.contains("name")) {
            if (!field_schema["name"].is_string() || field_schema["name"].empty())
                throw MantisException(400, "Invalid field name provided!");

            setName(field_schema["name"].get<std::string>());
        }

        if (field_schema.contains("type")) {
            if (!field_schema["type"].is_string() || field_schema["type"].empty())
                throw MantisException(400, "Invalid field name provided!");

            setType(field_schema["type"].get<std::string>());
        }

        if (field_schema.contains("required")) {
            if (!field_schema["required"].is_boolean())
                throw MantisException(400, "Expected a bool for field property `required`.");

            setRequired(field_schema["required"].get<bool>());
        }

        // Primary Key Flag
        if (field_schema.contains("primary_key")) {
            if (!field_schema["primary_key"].is_boolean())
                throw MantisException(400, "Expected a bool for field property `primary_key`.");

            setIsPrimaryKey(field_schema["primary_key"].get<bool>());
        }

        // System Flag
        if (field_schema.contains("system")) {
            if (!field_schema["system"].is_boolean())
                throw MantisException(400, "Expected a bool for field property `system`.");

            setIsSystem(field_schema["system"].get<bool>());
        }

        // Unique Flag
        if (field_schema.contains("unique")) {
            if (!field_schema["unique"].is_boolean())
                throw MantisException(400, "Expected a bool for field property `unique`.");

            setIsUnique(field_schema["unique"].get<bool>());
        }

        // Constraints Object
        if (field_schema.contains("constraints")) {
            if (!(field_schema["constraints"].is_object() || field_schema["constraints"].is_null()))
                throw MantisException(400, "Expected an object or null for `constraint` property.");

            setConstraints(field_schema["constraints"].get<nlohmann::json>());
        }

        return *this;
    }

    std::string EntitySchemaField::id() const {
        return EntitySchemaField::genFieldId(m_name);
    }

    std::string EntitySchemaField::name() const { return m_name; }

    EntitySchemaField &EntitySchemaField::setName(const std::string &name) {
        if (trim(name).empty())
            throw MantisException(400, "Field name is required!");

        m_name = trim(name);
        return *this;
    }

    std::string EntitySchemaField::type() const { return m_type; }

    EntitySchemaField &EntitySchemaField::setType(const std::string &type) {
        if (type.empty())
            throw MantisException(400, "Field type is required, none provided!");

        if (!isValidFieldType(type))
            throw MantisException(400, "Unsupported field type `" + type + "`");

        m_type = type;
        return *this;
    }

    bool EntitySchemaField::required() const { return m_required; }

    EntitySchemaField &EntitySchemaField::setRequired(const bool required) {
        m_required = required;
        return *this;
    }

    bool EntitySchemaField::isPrimaryKey() const { return m_primaryKey; }

    EntitySchemaField &EntitySchemaField::setIsPrimaryKey(const bool pk) {
        m_primaryKey = pk;
        return *this;
    }

    bool EntitySchemaField::isSystem() const { return m_isSystem; }

    EntitySchemaField &EntitySchemaField::setIsSystem(const bool system) {
        m_isSystem = system;
        return *this;
    }

    bool EntitySchemaField::isUnique() const { return m_isUnique; }

    EntitySchemaField &EntitySchemaField::setIsUnique(const bool unique) {
        m_isUnique = unique;
        return *this;
    }

    nlohmann::json EntitySchemaField::constraints() const { return m_constraints; }

    nlohmann::json EntitySchemaField::constraint(const std::string &key) const {
        logger::trace("Field `{}`, Constraints: \n\t> ", m_name, m_constraints.dump());
        if (!m_constraints.contains(key)) {
            throw MantisException(404, "No constraint found for key `" + key + "`");
        }

        return m_constraints[key];
    }

    EntitySchemaField &EntitySchemaField::setConstraints(const nlohmann::json &opts) {
        nlohmann::json constraints = m_constraints.empty() || m_constraints.is_null()
                                         ? defaultConstraints()
                                         : m_constraints;

        if (opts.contains("validator") && (opts["validator"].is_string() || opts["validator"].is_null()))
            constraints["validator"] = opts["validator"];

        // Default value can be of any type, keep it generic here
        if (opts.contains("default_value"))
            constraints["default_value"] = opts["default_value"];

        // Minimum value should be either null or numeric type
        if (opts.contains("min_value") && (opts["min_value"].is_number() || opts["min_value"].is_null()))
            constraints["min_value"] = opts["min_value"];

        // Maximum value should be either null or numeric type
        if (opts.contains("max_value") && (opts["max_value"].is_number() || opts["max_value"].is_null()))
            constraints["max_value"] = opts["max_value"];

        // Update the constraint value
        m_constraints = constraints;

        // Return self for chaining purposes
        return *this;
    }

    nlohmann::json EntitySchemaField::toJSON() const {
        return {
            {"id", genFieldId(m_name)},
            {"name", m_name},
            {"type", m_type},
            {"required", m_required},
            {"primary_key", m_primaryKey},
            {"system", m_isSystem},
            {"unique", m_isUnique},
            {"constraints", m_constraints}
        };
    }

    soci::db_type EntitySchemaField::toSociType() const {
        return EntitySchemaField::toSociType(m_type);
    }

    soci::db_type EntitySchemaField::toSociType(const std::string &type) {
        if (trim(type).empty())
            throw MantisException(400, "Field type is required, none provided!");

        if (type == "xml")
            return soci::db_xml;
        if (type == "double")
            return soci::db_double;
        if (type == "date")
            return soci::db_date;
        if (type == "int8")
            return soci::db_int8;
        if (type == "uint8")
            return soci::db_uint8;
        if (type == "int16")
            return soci::db_int16;
        if (type == "uint16")
            return soci::db_uint16;
        if (type == "int32")
            return soci::db_int32;
        if (type == "uint32")
            return soci::db_uint32;
        if (type == "int64")
            return soci::db_int64;
        if (type == "uint64")
            return soci::db_uint64;
        if (type == "blob")
            return soci::db_blob;
        if (type == "bool")
            return soci::db_uint16;
        if (type == "json" || type == "string" || type == "file" || type == "files")
            return soci::db_string;

        throw MantisException(400, "Unsupported field type `" + type + "`");
    }

    std::optional<std::string> EntitySchemaField::validate() const {
        if (m_name.empty()) return "Entity field name is empty";
        if (m_type.empty()) return "Entity field type is empty";
        return std::nullopt;
    }

    bool EntitySchemaField::isValidFieldType(const std::string &type) {
        const auto it = std::ranges::find(defaultEntityFieldTypes(), type);
        return it != defaultEntityFieldTypes().end();
    }

    std::string EntitySchemaField::genFieldId(const std::string &id) {
        return "mbf_" + std::to_string(std::hash<std::string>{}(id));
    }

    const nlohmann::json & EntitySchemaField::defaultConstraints() {
        static const nlohmann::json default_constraints = {
            {"min_value", nullptr},
            {"max_value", nullptr},
            {"validator", nullptr},
            {"default_value", nullptr}
        };
        return default_constraints;
    }
} // mantis
