#include "../../../include/mantisbase/core/models/entity.h"
#include "../../../include/mantisbase/core/models/entity_schema.h"
#include "../../../include/mantisbase/core/models/entity_schema_field.h"
#include "../../../include/mantisbase/utils/uuidv7.h"
#include "mantisbase/utils/soci_wrappers.h"

namespace mb {
    MbEntity::MbEntity(const MantisBase &app, const nlohmann::json &schema)
    : MbAppMixin(app) {
        logger().trace("MbEntity", "MbEntity Creation", "Creating MbEntity from JSON Schema", schema);

        if (!schema.contains("name") || !schema["name"].is_string() || schema["name"].empty())
            throw MbException(400, "Missing required entity `name` in schema!", schema.dump());

        if (!schema.contains("type") || !schema["type"].is_string() || schema["type"].empty())
            throw MbException(400, "Missing required entity `type` in schema!", schema.dump());

        // Ensure name is valid
        if (!MbEntitySchema::isValidEntityName(schema["name"].get<std::string>())) {
            throw MbException(400, "Invalid character in entity name", schema.dump());
        }

        if (!MbEntitySchema::isValidEntityType(schema["type"].get<std::string>())) {
            throw MbException(400, "Invalid entity type, expected `base`, `auth` or `view` only!",
                                  schema["type"].get<std::string>());
        }

        // Build the schema locally with all defaults filled in, then freeze it
        // into an immutable shared object (see m_schema docs).
        json s = schema;

        // Ensure we have defaults for any missing fields so that T& does not fail
        if (!s.contains("id"))
            s["id"] = MbEntitySchema::genEntityId(s.at("name").get<std::string>());
        if (!s.contains("system"))
            s["system"] = false;
        if (!s.contains("has_api"))
            s["has_api"] = true;

        json rules = s.contains("rules") ? schema["rules"] : json::object();
        if (!rules.contains("list"))
            rules = {{"list", MbAccessRule{}.toJSON()}};
        if (!rules.contains("get"))
            rules = {{"get", MbAccessRule{}.toJSON()}};
        if (!rules.contains("add"))
            rules = {{"add", MbAccessRule{}.toJSON()}};
        if (!rules.contains("update"))
            rules = {{"update", MbAccessRule{}.toJSON()}};
        if (!rules.contains("delete"))
            rules = {{"delete", MbAccessRule{}.toJSON()}};

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

    MbEntity::MbEntity(const MantisBase &app, const std::string &name, const std::string &type)
        : MbEntity(app, {{"name", name}, {"type", type}}) {
        // Ensure name is valid
        if (!MbEntitySchema::isValidEntityName(name)) {
            throw MbException(400, "Invalid entity name, expected alphanumeric + _ only!", name);
        }

        if (!MbEntitySchema::isValidEntityType(type)) {
            throw MbException(400, "Invalid entity type, expected `base`, `auth` or `view` only!", type);
        }
    }

    std::string MbEntity::id() const {
        return m_schema->at("id").get<std::string>();
    }

    std::string MbEntity::name() const {
        return m_schema->at("name").get<std::string>();
    }

    std::string MbEntity::type() const {
        return m_schema->at("type").get<std::string>();
    }

    bool MbEntity::isSystem() const {
        return m_schema->contains("system") ? m_schema->at("system").get<bool>() : false;
    }

    bool MbEntity::hasApi() const {
        return m_schema->contains("has_api") ? m_schema->at("has_api").get<bool>() : false;
    }

    std::string MbEntity::viewQuery() const {
        if (type() != "view")
            throw MbException(500, "View Query only allowed for `view` types!");

        if (!m_schema->contains("view_query"))
            throw std::invalid_argument("Missing view_query statement!");

        return m_schema->at("view_query").get<std::string>();
    }

    const std::vector<json> &MbEntity::fields() const {
        return m_schema->at("fields").get_ref<const std::vector<json> &>();
    }

    std::optional<json> MbEntity::field(const std::string &field_name) const {
        if (!m_schema->contains("fields")) return std::nullopt;
        for (auto _field: (*m_schema)["fields"]) {
            if (_field.contains("name") && _field["name"].get<std::string>() == field_name)
                return _field;
        }
        return std::nullopt;
    }

    std::optional<json> MbEntity::hasField(const std::string &field_name) const {
        return field(field_name).has_value();
    }

    const json &MbEntity::rules() const {
        return (*m_schema)["rules"];
    }

    MbAccessRule MbEntity::listRule() const {
        return MbAccessRule::fromJSON(rules()["list"]);
    }

    MbAccessRule MbEntity::getRule() const {
        return MbAccessRule::fromJSON(rules()["get"]);
    }

    MbAccessRule MbEntity::addRule() const {
        return MbAccessRule::fromJSON(rules()["add"]);
    }

    MbAccessRule MbEntity::updateRule() const {
        return MbAccessRule::fromJSON(rules()["update"]);
    }

    MbAccessRule MbEntity::deleteRule() const {
        return MbAccessRule::fromJSON(rules()["delete"]);
    }

    const json &MbEntity::schema() const { return *m_schema; }

    std::optional<json> MbEntity::findField(const std::string &field_name) const {
        if (field_name.empty()) return std::nullopt;

        for (auto field: fields()) {
            if (field.value("name", "") == field_name) return field;
        }

        return std::nullopt;
    }
} // mantis
