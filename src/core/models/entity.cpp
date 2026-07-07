#include "../../../include/mantisbase/core/models/entity.h"
#include "../../../include/mantisbase/core/models/entity_schema.h"
#include "../../../include/mantisbase/core/models/entity_schema_field.h"
#include "../../../include/mantisbase/utils/uuidv7.h"
#include "mantisbase/utils/soci_wrappers.h"

namespace mb {
    Entity::Entity(const MantisBase &app, const nlohmann::json &schema) : m_app(&app) {
        LogOrigin::entityTrace("Entity Creation", "Creating Entity from JSON Schema", schema);

        if (!schema.contains("name") || !schema["name"].is_string() || schema["name"].empty())
            throw MantisException(400, "Missing required entity `name` in schema!", schema.dump());

        if (!schema.contains("type") || !schema["type"].is_string() || schema["type"].empty())
            throw MantisException(400, "Missing required entity `type` in schema!", schema.dump());

        // Ensure name is valid
        if (!EntitySchema::isValidEntityName(schema["name"].get<std::string>())) {
            throw MantisException(400, "Invalid character in entity name", schema.dump());
        }

        if (!EntitySchema::isValidEntityType(schema["type"].get<std::string>())) {
            throw MantisException(400, "Invalid entity type, expected `base`, `auth` or `view` only!",
                                  schema["type"].get<std::string>());
        }

        // Build the schema locally with all defaults filled in, then freeze it
        // into an immutable shared object (see m_schema docs).
        json s = schema;

        // Ensure we have defaults for any missing fields so that T& does not fail
        if (!s.contains("id"))
            s["id"] = EntitySchema::genEntityId(s.at("name").get<std::string>());
        if (!s.contains("system"))
            s["system"] = false;
        if (!s.contains("has_api"))
            s["has_api"] = true;

        json rules = s.contains("rules") ? schema["rules"] : json::object();
        if (!rules.contains("list"))
            rules = {{"list", AccessRule{}.toJSON()}};
        if (!rules.contains("get"))
            rules = {{"get", AccessRule{}.toJSON()}};
        if (!rules.contains("add"))
            rules = {{"add", AccessRule{}.toJSON()}};
        if (!rules.contains("update"))
            rules = {{"update", AccessRule{}.toJSON()}};
        if (!rules.contains("delete"))
            rules = {{"delete", AccessRule{}.toJSON()}};

        s["rules"] = rules;

        if (s.at("type").get<std::string>() == "view") {
            if (!s.contains("view_query"))
                s["view_query"] = "";
        } else {
            if (!s.contains("fields"))
                s["fields"] = json::array();
        }

        m_schema = std::make_shared<const json>(std::move(s));
    }

    Entity::Entity(const MantisBase &app, const std::string &name, const std::string &type)
        : Entity(app, {{"name", name}, {"type", type}}) {
        // Ensure name is valid
        if (!EntitySchema::isValidEntityName(name)) {
            throw MantisException(400, "Invalid entity name, expected alphanumeric + _ only!", name);
        }

        if (!EntitySchema::isValidEntityType(type)) {
            throw MantisException(400, "Invalid entity type, expected `base`, `auth` or `view` only!", type);
        }
    }

    const MantisBase &Entity::app() const {
        return *m_app;
    }

    std::string Entity::id() const {
        return m_schema->at("id").get<std::string>();
    }

    std::string Entity::name() const {
        return m_schema->at("name").get<std::string>();
    }

    std::string Entity::type() const {
        return m_schema->at("type").get<std::string>();
    }

    bool Entity::isSystem() const {
        return m_schema->contains("system") ? m_schema->at("system").get<bool>() : false;
    }

    bool Entity::hasApi() const {
        return m_schema->contains("has_api") ? m_schema->at("has_api").get<bool>() : false;
    }

    std::string Entity::viewQuery() const {
        if (type() != "view")
            throw MantisException(500, "View Query only allowed for `view` types!");

        if (m_schema->contains("view_query"))
            throw std::invalid_argument("Missing view_query statement!");

        return m_schema->at("view_query").get<std::string>();
    }

    const std::vector<json> &Entity::fields() const {
        return m_schema->at("fields").get_ref<const std::vector<json> &>();
    }

    std::optional<json> Entity::field(const std::string &field_name) const {
        if (!m_schema->contains("fields")) return std::nullopt;
        for (auto _field: (*m_schema)["fields"]) {
            if (_field.contains("name") && _field["name"].get<std::string>() == field_name)
                return _field;
        }
        return std::nullopt;
    }

    std::optional<json> Entity::hasField(const std::string &field_name) const {
        return field(field_name).has_value();
    }

    const json &Entity::rules() const {
        return (*m_schema)["rules"];
    }

    AccessRule Entity::listRule() const {
        return AccessRule::fromJSON(rules()["list"]);
    }

    AccessRule Entity::getRule() const {
        return AccessRule::fromJSON(rules()["get"]);
    }

    AccessRule Entity::addRule() const {
        return AccessRule::fromJSON(rules()["add"]);
    }

    AccessRule Entity::updateRule() const {
        return AccessRule::fromJSON(rules()["update"]);
    }

    AccessRule Entity::deleteRule() const {
        return AccessRule::fromJSON(rules()["delete"]);
    }

    const json &Entity::schema() const { return *m_schema; }

    std::optional<json> Entity::findField(const std::string &field_name) const {
        if (field_name.empty()) return std::nullopt;

        for (auto field: fields()) {
            if (field.value("name", "") == field_name) return field;
        }

        return std::nullopt;
    }
} // mantis
