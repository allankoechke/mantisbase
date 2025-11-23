//
// Created by codeart on 13/11/2025.
//

#include "../../../include/mantisbase/core/models/entity_schema_field.h"
#include "../../../include/mantisbase/utils/utils.h"

namespace mantis {
    EntitySchemaField::EntitySchemaField(std::string field_name, std::string field_type)
        : m_name(std::move(field_name)), m_type(std::move(field_type)) {
    }

    EntitySchemaField::EntitySchemaField(const nlohmann::json &field_schema) {
        if (!field_schema.contains("name") || field_schema["name"].is_null())
            throw std::invalid_argument("Field name is required!");
        setName(field_schema["name"].get<std::string>());

        if (!field_schema.contains("type") || field_schema["type"].is_null())
            throw std::invalid_argument("Field type is required!");
        setType(field_schema["type"].get<std::string>());

        if (field_schema.contains("id") && field_schema["id"].is_string())
            setId(field_schema.at("id").get<std::string>());

        if (field_schema.contains("required") && field_schema["required"].is_boolean())
            setRequired(field_schema["required"].get<bool>());

        if (field_schema.contains("primary_key") && field_schema["primary_key"].is_boolean())
            setIsPrimaryKey(field_schema["primary_key"].get<bool>());

        if (field_schema.contains("system") && field_schema["system"].is_boolean())
            setIsSystem(field_schema["system"].get<bool>());

        if (field_schema.contains("unique") && field_schema["unique"].is_boolean())
            setIsUnique(field_schema["unique"].get<bool>());

        if (field_schema.contains("constraints") && field_schema["constraints"].is_object())
            setConstraints(field_schema["constraints"].get<nlohmann::json>());
    }

    void EntitySchemaField::updateWith(const nlohmann::json &field_schema) {
        if (field_schema.contains("name") && !field_schema["name"].is_null())
            setName(field_schema["name"].get<std::string>());

        if (field_schema.contains("type") && !field_schema["type"].is_null())
            setType(field_schema["type"].get<std::string>());

        if (field_schema.contains("id") && field_schema["id"].is_string())
            setId(field_schema.at("id").get<std::string>());

        if (field_schema.contains("required") && field_schema["required"].is_boolean())
            setRequired(field_schema["required"].get<bool>());

        if (field_schema.contains("primary_key") && field_schema["primary_key"].is_boolean())
            setIsPrimaryKey(field_schema["primary_key"].get<bool>());

        if (field_schema.contains("system") && field_schema["system"].is_boolean())
            setIsSystem(field_schema["system"].get<bool>());

        if (field_schema.contains("unique") && field_schema["unique"].is_boolean())
            setRequired(field_schema["unique"].get<bool>());

        if (field_schema.contains("constraints") && field_schema["constraints"].is_object())
            setConstraints(field_schema["constraints"].get<nlohmann::json>());
    }

    std::string EntitySchemaField::id() const {
        return m_id.empty() ? EntitySchemaField::genFieldId(m_name) : m_id;
    }

    EntitySchemaField & EntitySchemaField::setId(const std::string &id) {
        m_name = trim(id);
        return *this;
    }

    std::string EntitySchemaField::name() const { return m_name; }

    EntitySchemaField &EntitySchemaField::setName(const std::string &name) {
        if (trim(name).empty()) throw std::invalid_argument("Field name is required!");
        m_name = trim(name);
        return *this;
    }

    std::string EntitySchemaField::type() const { return m_type; }

    EntitySchemaField &EntitySchemaField::setType(const std::string &type) {
        if (type.empty())
            throw std::invalid_argument("Field type is required!");

        if (!isValidFieldType(type))
            throw std::invalid_argument("Invalid field type `" + type + "`");

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

    nlohmann::json EntitySchemaField::constraints(const std::string &key) const {
        return m_constraints.contains(key) ? m_constraints[key] : nullptr;
    }

    EntitySchemaField &EntitySchemaField::setConstraints(const nlohmann::json &opts) {
        if (m_constraints.is_null())
            m_constraints = {
                {"min_value", nullptr},
                {"max_value", nullptr},
                {"validator", nullptr},
                {"default_value", nullptr}
            };

        nlohmann::json constraints = nlohmann::json::object();
        if (opts.contains("validator"))
            m_constraints["validator"] = opts["validator"];

        if (opts.contains("default_value"))
            m_constraints["default_value"] = opts["default_value"];

        if (opts.contains("min_value") && opts["min_value"].is_number())
            m_constraints["min_value"] = opts["min_value"];

        if (opts.contains("max_value") && opts["max_value"].is_number())
            m_constraints["max_value"] = opts["max_value"];

        return *this;
    }

    nlohmann::json EntitySchemaField::toJson() const {
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
        if (type == "json")
            return soci::db_string;
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
        if (type == "string" || type == "file" || type == "files")
            return soci::db_string;

        throw std::invalid_argument("Unknown field type");
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
} // mantis
