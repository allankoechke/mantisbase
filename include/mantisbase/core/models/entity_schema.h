/**
 * @file entity_schema.h
 * @brief Entity schema builder and management class.
 *
 * Provides methods to define, validate, and manage database table schemas
 * including fields, access rules, and DDL generation.
 */

#ifndef MANTISBASE_ENTITY_SCHEMA_H
#define MANTISBASE_ENTITY_SCHEMA_H

#include <vector>
#include <ranges>
#include <algorithm>

#include "entity.h"
#include "entity_schema_field.h"
#include "access_rules.h"

namespace mb {
    /**
     * @brief Builder class for creating and managing database table schemas.
     *
     * EntitySchema provides a fluent interface for defining table structures,
     * fields, access rules, and converting to/from JSON and DDL.
     *
     * @code
     * // Create a new schema
     * EntitySchema schema("users", "base");
     * schema.addField(EntitySchemaField("name", "string").setRequired(true));
     * schema.addField(EntitySchemaField("email", "string").setIsUnique(true));
     * schema.setListRule(AccessRule("custom", "auth.id != \"\""));
     *
     * // Convert to Entity for database operations
     * Entity entity = schema.toEntity();
     * @endcode
     */
    class EntitySchema {
    public:
        /**
         * @brief Default constructor (creates empty schema).
         */
        EntitySchema() = default;
        
        /**
         * @brief Construct schema with name and type.
         * @param entity_name Table name
         * @param entity_type Entity type ("base", "auth", or "view"), defaults to "base"
         */
        explicit EntitySchema(const std::string &entity_name, const std::string &entity_type = "base");

        // Allow copy constructors & assignment ops for easy cloning
        EntitySchema(const EntitySchema &);
        EntitySchema &operator=(const EntitySchema &);

        // Move operators ...
        EntitySchema(EntitySchema &&) noexcept = default;
        EntitySchema &operator=(EntitySchema &&) noexcept = default;

        ~EntitySchema();

        /**
         * @brief Compare two schemas for equality.
         * @param other Schema to compare with
         * @return true if schemas are equal
         */
        bool operator==(const EntitySchema &other) const;

        /**
         * @brief Create schema from JSON object.
         * @param entity_schema JSON schema object
         * @return EntitySchema instance
         */
        static EntitySchema fromSchema(const json &entity_schema);
        
        /**
         * @brief Create schema from existing Entity.
         * @param entity Entity to convert from
         * @return EntitySchema instance
         */
        static EntitySchema fromEntity(const Entity &entity);
        
        /**
         * @brief Convert schema to Entity for database operations.
         * @return Entity instance
         */
        [[nodiscard]] Entity toEntity() const;

        // ----------- SCHEMA METHODS ----------- //
        /**
         * @brief Get schema unique identifier.
         * @return Schema ID string
         */
        [[nodiscard]] std::string id() const;

        /**
         * @brief Get table name.
         * @return Table name
         */
        [[nodiscard]] std::string name() const;

        /**
         * @brief Set table name (fluent interface).
         * @param name New table name
         * @return Reference to self for chaining
         */
        EntitySchema &setName(const std::string &name);

        /**
         * @brief Get entity type.
         * @return Entity type ("base", "auth", or "view")
         */
        [[nodiscard]] std::string type() const;

        /**
         * @brief Set entity type (fluent interface).
         * @param type Entity type
         * @return Reference to self for chaining
         */
        EntitySchema &setType(const std::string &type);

        /**
         * @brief Check if API endpoints are enabled.
         * @return true if API enabled
         */
        [[nodiscard]] bool hasApi() const;

        /**
         * @brief Enable/disable API endpoints (fluent interface).
         * @param hasApi API enabled flag
         * @return Reference to self for chaining
         */
        EntitySchema &setHasApi(const bool &hasApi);

        /**
         * @brief Check if this is a system table.
         * @return true if system table
         */
        [[nodiscard]] bool isSystem() const;

        /**
         * @brief Set system table flag (fluent interface).
         * @param isSystem System table flag
         * @return Reference to self for chaining
         */
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

        /**
         * @brief Get all field definitions.
         * @return Vector of EntitySchemaField objects
         */
        [[nodiscard]] std::vector<EntitySchemaField> fields() const;

        /**
         * @brief Add a field to the schema (fluent interface).
         * @param field Field to add
         * @return Reference to self for chaining
         */
        EntitySchema &addField(const EntitySchemaField &field);

        /**
         * @brief Remove a field by name.
         * @param field_name Field name to remove
         * @return true if field was removed, false if not found
         */
        bool removeField(const std::string &field_name);

        /**
         * @brief Get field by name (mutable reference).
         * @param field_name Field name to lookup
         * @return Reference to EntitySchemaField
         * @throws std::runtime_error if field not found
         */
        EntitySchemaField &field(const std::string &field_name);

        /**
         * @brief Get field by ID (mutable reference).
         * @param field_id Field ID to lookup
         * @return Reference to EntitySchemaField
         * @throws std::runtime_error if field not found
         */
        EntitySchemaField &fieldById(const std::string &field_id);

        /**
         * @brief Check if field exists by name.
         * @param field_name Field name to check
         * @return true if field exists
         */
        [[nodiscard]] bool hasField(const std::string &field_name) const;

        /**
         * @brief Check if field exists by ID.
         * @param field_id Field ID to check
         * @return true if field exists
         */
        [[nodiscard]] bool hasFieldById(const std::string &field_id) const;

        /**
         * @brief Get SQL query for view type entities.
         * @return View SQL query string
         */
        [[nodiscard]] std::string viewQuery() const;

        /**
         * @brief Set SQL query for view type (fluent interface).
         * @param viewQuery SQL SELECT query string
         * @return Reference to self for chaining
         */
        EntitySchema &setViewQuery(const std::string &viewQuery);

        /**
         * @brief Update schema with new JSON data (merges fields).
         * @param new_data JSON object with updates
         */
        void updateWith(const nlohmann::json &new_data);

        // ----------- SCHEMA CONVERSION ----------- //
        /**
         * @brief Convert schema to JSON representation.
         * @return JSON object with schema data
         */
        [[nodiscard]] json toJSON() const;

        /**
         * @brief Generate SQL DDL (CREATE TABLE) statement.
         * @return SQL DDL string
         */
        [[nodiscard]] std::string toDDL() const;

        /**
         * @brief Get default SQL value for a field type.
         * @param type Field type string
         * @param v JSON value (optional)
         * @return Default SQL value string
         */
        static std::string toDefaultSqlValue(const std::string &type, const nlohmann::json &v);

        /**
         * @brief Dump schema as formatted string (for debugging).
         * @return Formatted schema string
         */
        [[nodiscard]] std::string dump() const;

        // ----------- SCHEMA CRUD ----------- //
        /**
         * @brief List all tables from database.
         * @param opts Optional parameters (pagination, etc.)
         * @return JSON array of table schemas
         */
        static nlohmann::json listTables(const nlohmann::json &opts = nlohmann::json::object());

        /**
         * @brief Get table schema by ID from database.
         * @param table_id Table identifier
         * @return JSON schema object
         */
        static nlohmann::json getTable(const std::string &table_id);

        /**
         * @brief Create table in database from schema.
         * @param new_table Schema to create
         * @return JSON object with created table data
         */
        static nlohmann::json createTable(const EntitySchema &new_table);

        /**
         * @brief Update existing table schema in database.
         * @param table_id Table identifier
         * @param new_schema Updated schema JSON
         * @return JSON object with updated table data
         */
        static nlohmann::json updateTable(const std::string &table_id, const nlohmann::json &new_schema);

        /**
         * @brief Drop table from database.
         * @param original_table Schema of table to drop
         */
        static void dropTable(const EntitySchema &original_table);

        /**
         * @brief Drop table by ID from database.
         * @param table_id Table identifier to drop
         */
        static void dropTable(const std::string &table_id);

        /**
         * @brief Check if table exists by name.
         * @param table_name Table name to check
         * @return true if table exists
         */
        static bool tableExists(const std::string &table_name);

        /**
         * @brief Check if table exists (by schema name).
         * @param table Schema to check
         * @return true if table exists
         */
        static bool tableExists(const EntitySchema &table);

        /**
         * @brief Generate unique entity ID from name.
         * @param entity_name Entity name
         * @return Generated ID string
         */
        static std::string genEntityId(const std::string &entity_name);

        /**
         * @brief Validate schema (static method).
         * @param table_schema Schema to validate
         * @return Optional error message if validation fails
         */
        static std::optional<std::string> validate(const EntitySchema &table_schema);

        /**
         * @brief Validate this schema instance.
         * @return Optional error message if validation fails
         */
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
} // mb

#endif //MANTISBASE_ENTITY_SCHEMA_H
