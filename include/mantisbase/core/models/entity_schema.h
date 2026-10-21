/**
 * @file entity_schema.h
 * @brief MbEntity schema builder and management class.
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
#include "mantisbase/core/types.h"

namespace mb {
    /**
     * @brief Builder class for creating and managing database table schemas.
     *
     * MbEntitySchema provides a fluent interface for defining table structures,
     * fields, access rules, and converting to/from JSON and DDL.
     *
     * @code
     * auto app = MantisBase::create();
     * MbEntitySchema schema(*app, "users", "base");
     * schema.addField(MbEntitySchemaField("name", "string").setRequired(true));
     * schema.addField(MbEntitySchemaField("email", "string").setIsUnique(true));
     * schema.setListRule(MbAccessRule("auth", "", "users"));
     *
     * MbEntity entity = schema.toEntity();
     * @endcode
     */
    class MANTISBASE_API MbEntitySchema: public MbAppMixin {
    public:
        /**
         * @brief Construct an empty schema bound to its application.
         *
         * Used by the fromSchema()/fromEntity() builders before fields are added.
         * The app is injected at construction so DDL/validation can always reach
         * the database without a separate bind step to forget.
         *
         * @param app Owning application. Stored non-owning; must outlive the schema.
         */
        explicit MbEntitySchema(const MantisBase &app);

        /**
         * @brief Construct schema with name and type, bound to its application.
         * @param app Owning application (see the app-only constructor).
         * @param entity_name Table name
         * @param entity_type MbEntity type ("base", "auth", or "view"), defaults to "base"
         */
        MbEntitySchema(const MantisBase &app, const std::string &entity_name, const std::string &entity_type = "base");

        // Allow copy constructors & assignment ops for easy cloning
        MbEntitySchema(const MbEntitySchema &);
        MbEntitySchema &operator=(const MbEntitySchema &);

        // Move operators (move ctor can copy the app ref; move assignment
        // keeps the bound app and moves the rest, mirroring copy assignment)
        MbEntitySchema(MbEntitySchema &&) noexcept = default;
        MbEntitySchema &operator=(MbEntitySchema &&) noexcept;

        ~MbEntitySchema();

        /**
         * @brief Compare two schemas for equality.
         * @param other Schema to compare with
         * @return true if schemas are equal
         */
        bool operator==(const MbEntitySchema &other) const;

        /**
         * @brief Create schema from JSON object, bound to an application.
         * @param app Owning application (db access for DDL/validation).
         * @param entity_schema JSON schema object
         * @return MbEntitySchema instance
         */
        static MbEntitySchema fromSchema(const MantisBase &app, const json &entity_schema);

        /**
         * @brief Create schema from existing MbEntity, bound to an application.
         * @param app A const ref to MantisBase running instance
         * @param entity MbEntity to convert from
         * @return MbEntitySchema instance
         */
        static MbEntitySchema fromEntity(const MantisBase &app, const MbEntity &entity);
        
        /**
         * @brief Convert schema to MbEntity for database operations.
         * @return MbEntity instance
         */
        [[nodiscard]] MbEntity toEntity() const;

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
        MbEntitySchema &setName(const std::string &name);

        /**
         * @brief Get entity type.
         * @return MbEntity type ("base", "auth", or "view")
         */
        [[nodiscard]] std::string type() const;

        /**
         * @brief Set entity type (fluent interface).
         * @param type MbEntity type
         * @return Reference to self for chaining
         */
        MbEntitySchema &setType(const std::string &type);

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
        MbEntitySchema &setHasApi(const bool &hasApi);

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
        MbEntitySchema &setSystem(const bool &isSystem);

        /** @return List (collection) access rule for this schema. */
        [[nodiscard]] MbAccessRule listRule() const;

        MbEntitySchema &setListRule(const MbAccessRule &listRule);

        /** @return Get (single-record) access rule. */
        [[nodiscard]] MbAccessRule getRule() const;

        MbEntitySchema &setGetRule(const MbAccessRule &getRule);

        /** @return Create access rule. */
        [[nodiscard]] MbAccessRule addRule() const;

        MbEntitySchema &setAddRule(const MbAccessRule &addRule);

        /** @return Update access rule. */
        [[nodiscard]] MbAccessRule updateRule() const;

        MbEntitySchema &setUpdateRule(const MbAccessRule &updateRule);

        /** @return Delete access rule. */
        [[nodiscard]] MbAccessRule deleteRule() const;

        MbEntitySchema &setDeleteRule(const MbAccessRule &deleteRule);

        /**
         * @brief Get all field definitions.
         * @return Vector of MbEntitySchemaField objects
         */
        [[nodiscard]] std::vector<MbEntitySchemaField> fields() const;

        /**
         * @brief Add a field to the schema (fluent interface).
         * @param field Field to add
         * @return Reference to self for chaining
         */
        MbEntitySchema &addField(const MbEntitySchemaField &field);

        /**
         * @brief Remove a field by name.
         * @param field_name Field name to remove
         * @return true if field was removed, false if not found
         */
        bool removeField(const std::string &field_name);

        /**
         * @brief Get field by name (mutable reference).
         * @param field_name Field name to lookup
         * @return Reference to MbEntitySchemaField
         * @throws std::runtime_error if field not found
         */
        MbEntitySchemaField &field(const std::string &field_name);

        /**
         * @brief Get field by ID (mutable reference).
         * @param field_id Field ID to lookup
         * @return Reference to MbEntitySchemaField
         * @throws std::runtime_error if field not found
         */
        MbEntitySchemaField &fieldById(const std::string &field_id);

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

        /** @return SQL SELECT for view-type entities. */
        [[nodiscard]] std::string viewQuery() const;

        MbEntitySchema &setViewQuery(const std::string &viewQuery);

        /** @return Index definitions declared on this schema. */
        [[nodiscard]] const std::vector<IndexDefinition> &indexes() const;

        MbEntitySchema &addIndex(const IndexDefinition &index);

        bool removeIndex(const std::string &index_name);

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

        /** @return `CREATE INDEX` statements for declared secondary indexes. */
        [[nodiscard]] std::vector<std::string> indexDDL() const;

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
         * @param app A const ref to MantisBase running instance
         * @param opts Optional parameters (pagination, etc.)
         * @return JSON array of table schemas
         */
        static nlohmann::json listTables(const MantisBase &app, const nlohmann::json &opts = nlohmann::json::object());

        /**
         * @brief Get table schema by ID from database.
         * @param app Owning application (db access).
         * @param table_id Table identifier
         * @return JSON schema object
         */
        static nlohmann::json getTable(const MantisBase &app, const std::string &table_id);

        /**
         * @brief Create table in database from schema.
         * @param new_table Schema to create
         * @return JSON object with created table data
         */
        static nlohmann::json createTable(const MbEntitySchema &new_table);

        /**
         * @brief Update existing table schema in database.
         * @param app A const ref to MantisBase running instance
         * @param table_id Table identifier
         * @param new_schema Updated schema JSON
         * @return JSON object with updated table data
         */
        static nlohmann::json updateTable(const MantisBase &app, const std::string &table_id, const nlohmann::json &new_schema);

        /**
         * @brief Drop table from database.
         * @param original_table Schema of table to drop
         */
        static void dropTable(const MbEntitySchema &original_table);

        /**
         * @brief Drop table by ID from database.
         * @param app A const ref to MantisBase running instance
         * @param table_id Table identifier to drop
         */
        static void dropTable(const MantisBase &app, const std::string &table_id);

        /**
         * @brief Check if table exists by name.
         * @param app A const ref to MantisBase running instance
         * @param table_name Table name to check
         * @return true if table exists
         */
        static bool tableExists(const MantisBase &app, const std::string &table_name);

        /**
         * @brief Check if table exists (by schema name).
         * @param table Schema to check
         * @return true if table exists
         */
        static bool tableExists(const MbEntitySchema &table);

        /**
         * @brief Generate unique entity ID from name.
         * @param entity_name MbEntity name
         * @return Generated ID string
         */
        static std::string genEntityId(const std::string &entity_name);

        /**
         * @brief Validate schema (static method).
         * @param table_schema Schema to validate
         * @return Optional error message if validation fails
         */
        static std::optional<std::string> validate(const MbEntitySchema &table_schema);

        /**
         * @brief Validate this schema instance.
         * @return Optional error message if validation fails
         */
        [[nodiscard]] std::optional<std::string> validate() const;

        /**
         * @brief Validate this schema name to alphanum and _ only
         * @return Boolean true if all checks out
         */
        static bool isValidEntityName(const std::string& name);

        /**
         * @brief Validate this schema type to the 3 supported types
         * @return Boolean true if all checks out
         */
        static bool isValidEntityType(const std::string& type);

        /** @return Default field objects injected into `base` entity schemas. */
        static const std::vector<MbEntitySchemaField> &defaultBaseFieldsSchema();

        /** @return Default field objects injected into `auth` entity schemas. */
        static const std::vector<MbEntitySchemaField> &defaultAuthFieldsSchema();

    private:
        static std::string getFieldType(const std::string &type, std::shared_ptr<soci::session> sql,
                                        MbIntPrecision precision = MbIntPrecision::I32);

        void addFieldsIfNotExist(const std::string &type);

        std::string m_name;
        std::string m_type;
        std::string m_viewSqlQuery;
        bool m_isSystem = false;
        bool m_hasApi = true;
        std::vector<MbEntitySchemaField> m_fields;
        std::vector<IndexDefinition> m_indexes;
        MbAccessRule m_listRule, m_getRule, m_addRule, m_updateRule, m_deleteRule;
    };
} // mb

#endif //MANTISBASE_ENTITY_SCHEMA_H
