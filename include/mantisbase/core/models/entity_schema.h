#ifndef MANTISBASE_ENTITY_SCHEMA_H
#define MANTISBASE_ENTITY_SCHEMA_H

#include <vector>
#include <ranges>
#include <algorithm>

#include "entity.h"
#include "entity_schema_field.h"
#include "access_rules.h"

namespace mantis {
    class EntitySchema {
    public:
        EntitySchema() = default;

        explicit EntitySchema(const std::string &entity_name, const std::string &entity_type = "base");

        // Allow copy constructors & assignment ops for easy cloning
        EntitySchema(const EntitySchema &);

        EntitySchema &operator=(const EntitySchema &);

        // Move operators ...
        EntitySchema(EntitySchema &&) noexcept = default;

        EntitySchema &operator=(EntitySchema &&) noexcept = default;

        ~EntitySchema();

        static EntitySchema fromSchema(const json &entity_schema);

        static EntitySchema fromEntity(const Entity &entity);

        [[nodiscard]] Entity toEntity() const;

        // ----------- SCHEMA METHODS ----------- //
        [[nodiscard]] std::string id() const;

        [[nodiscard]] std::string name() const;

        EntitySchema &setName(const std::string &name);

        [[nodiscard]] std::string type() const;

        EntitySchema &setType(const std::string &type);

        [[nodiscard]] bool hasApi() const;

        EntitySchema &setHasApi(const bool &hasApi);

        [[nodiscard]] bool isSystem() const;

        EntitySchema &setSystem(const bool &isSystem);

        [[nodiscard]] AccessRule listRule() const;

        EntitySchema &setListRule(const AccessRule &listRule);

        [[nodiscard]] AccessRule getRule() const;

        EntitySchema &setGetRule(const AccessRule &getRule);

        [[nodiscard]] AccessRule addRule() const;

        EntitySchema &setAddRule(const AccessRule &addRule);

        [[nodiscard]] AccessRule updateRule() const;

        EntitySchema &setUpdateRule(const AccessRule &updateRule);

        [[nodiscard]] AccessRule deleteRule() const;

        EntitySchema &setDeleteRule(const AccessRule &deleteRule);

        [[nodiscard]] std::vector<EntitySchemaField> fields() const;

        EntitySchema &addField(const EntitySchemaField &field);

        bool removeField(const std::string &field_name);

        EntitySchemaField &field(const std::string &field_name);

        EntitySchemaField &fieldById(const std::string &field_id);

        [[nodiscard]] bool hasField(const std::string &field_name) const;

        [[nodiscard]] bool hasFieldById(const std::string &field_id) const;

        [[nodiscard]] std::string viewQuery() const;

        EntitySchema &setViewQuery(const std::string &viewQuery);

        void updateWith(const nlohmann::json &new_data);

        // ----------- SCHEMA CONVERSION ----------- //
        [[nodiscard]] json toJson() const;

        [[nodiscard]] std::string toDDL() const;

        static std::string toDefaultSqlValue(const std::string &type, const nlohmann::json &v);

        std::string dump() const;

        // ----------- SCHEMA CRUD ----------- //
        static nlohmann::json listTables(const nlohmann::json &opts = nlohmann::json::object());

        static nlohmann::json getTable(const std::string &table_id);

        static nlohmann::json createTable(const EntitySchema &new_table);

        static nlohmann::json updateTable(const std::string &table_id, const nlohmann::json &new_data);

        static void dropTable(const EntitySchema &original_table);

        static void dropTable(const std::string &table_id);

        static bool tableExists(const std::string &table_name);

        static bool tableExists(const EntitySchema &table);

        static std::string genEntityId(const std::string &entity_name);

        static std::optional<std::string> validate(const EntitySchema &table_schema);

        [[nodiscard]] std::optional<std::string> validate() const;

        // --------------- SCHEMA ROUTING ------------------ //
        [[nodiscard]] HandlerFn getOneRouteHandler() const;

        [[nodiscard]] HandlerFn getManyRouteHandler() const;

        [[nodiscard]] HandlerFn postRouteHandler() const;

        [[nodiscard]] HandlerFn patchRouteHandler() const;

        [[nodiscard]] HandlerFn deleteRouteHandler() const;

        void createEntityRoutes() const;

    private:
        static std::string getFieldType(const std::string &type, std::shared_ptr<soci::session> sql);

        void addFieldsIfNotExist(const std::string &type);

        static const std::vector<EntitySchemaField> &defaultBaseFieldsSchema();

        static const std::vector<EntitySchemaField> &defaultAuthFieldsSchema();

        std::string m_name;
        std::string m_type;
        std::string m_viewSqlQuery;
        bool m_isSystem = false;
        bool m_hasApi = true;
        std::vector<EntitySchemaField> m_fields;
        AccessRule m_listRule, m_getRule, m_addRule, m_updateRule, m_deleteRule;
    };
} // mantis

#endif //MANTISBASE_ENTITY_SCHEMA_H
