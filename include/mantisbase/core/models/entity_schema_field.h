/**
 * @file entity_schema_field.h
 * Entity schema field definition and validation.
 *
 * Represents a single field in a database table schema with type,
 * constraints, and validation rules.
 */

#ifndef MANTISBASE_ENTITY_SCHEMA_FIELD_H
#define MANTISBASE_ENTITY_SCHEMA_FIELD_H

#include <optional>
#include <string>

#include "nlohmann/json.hpp"
#include "nlohmann/json.hpp"
#include "soci/soci-backend.h"

namespace mb {
    /**
     * Represents a single field in a database table schema.
     *
     * Defines field properties including name, type, constraints, and validation rules.
     * Supports fluent interface for building field definitions.
     *
     * @code
     * // Create a required string field
     * EntitySchemaField nameField("name", "string");
     * nameField.setRequired(true);
     *
     * // Create a unique email field
     * EntitySchemaField emailField("email", "string");
     * emailField.setIsUnique(true).setRequired(true);
     * @endcode
     */
    class EntitySchemaField {
    public:
        /**
         * Construct field with name and type.
         * @param field_name Field name
         * @param field_type Field type (e.g., "string", "int32", "bool", "date")
         */
        EntitySchemaField(std::string field_name, std::string field_type);

        /**
         * Construct field from JSON schema object.
         * @param field_schema JSON object with field definition
         */
        explicit EntitySchemaField(const nlohmann::json &field_schema);

        EntitySchemaField(const EntitySchemaField &other);
        bool operator==(const EntitySchemaField &other) const;

        // ----------------- STATIC GLOBAL METHODS ---------------------- //

        static const std::vector<std::string> &defaultBaseFields();

        static const std::vector<std::string> &defaultAuthFields();

        static const std::vector<std::string> &defaultEntityFieldTypes();

        // ----------------- SCHEMA FIELD METHODS ---------------------- //

        /**
         * Get field unique identifier.
         * @return Field ID string
         */
        [[nodiscard]] std::string id() const;

        /**
         * Get field name.
         * @return Field name
         */
        [[nodiscard]] std::string name() const;

        /**
         * Set field name (fluent interface).
         * @param name New field name
         * @return Reference to self for chaining
         */
        EntitySchemaField &setName(const std::string &name);

        /**
         * Get field type.
         * @return Field type string
         */
        [[nodiscard]] std::string type() const;

        /**
         * Set field type (fluent interface).
         * @param type Field type (e.g., "string", "int32", "bool")
         * @return Reference to self for chaining
         */
        EntitySchemaField &setType(const std::string &type);

        /**
         * Check if field is required.
         * @return true if required
         */
        [[nodiscard]] bool required() const;

        /**
         * Set required flag (fluent interface).
         * @param required Required flag
         * @return Reference to self for chaining
         */
        EntitySchemaField &setRequired(bool required);

        /**
         * Check if field is primary key.
         * @return true if primary key
         */
        [[nodiscard]] bool isPrimaryKey() const;

        /**
         * Set primary key flag (fluent interface).
         * @param pk Primary key flag
         * @return Reference to self for chaining
         */
        EntitySchemaField &setIsPrimaryKey(bool pk);

        /**
         * Check if field is system field.
         * @return true if system field
         */
        [[nodiscard]] bool isSystem() const;

        /**
         * Set system field flag (fluent interface).
         * @param system System field flag
         * @return Reference to self for chaining
         */
        EntitySchemaField &setIsSystem(bool system);

        /**
         * Check if field has unique constraint.
         * @return true if unique
         */
        [[nodiscard]] bool isUnique() const;

        /**
         * Set unique constraint (fluent interface).
         * @param unique Unique flag
         * @return Reference to self for chaining
         */
        EntitySchemaField &setIsUnique(bool unique);

        /**
         * Check if field is a foreign key.
         * @return true if foreign key
         */
        [[nodiscard]] bool isForeignKey() const;

        /**
         * Get foreign key reference table name.
         * @return Reference table name, empty if not a foreign key
         */
        [[nodiscard]] std::string foreignKeyTable() const;

        /**
         * Get foreign key reference column name.
         * @return Reference column name, defaults to "id" if not specified
         */
        [[nodiscard]] std::string foreignKeyColumn() const;

        /**
         * Get foreign key update policy.
         * @return Update policy ("CASCADE", "SET NULL", "RESTRICT", "NO ACTION", "SET DEFAULT")
         */
        [[nodiscard]] std::string foreignKeyOnUpdate() const;

        /**
         * Get foreign key delete policy.
         * @return Delete policy ("CASCADE", "SET NULL", "RESTRICT", "NO ACTION", "SET DEFAULT")
         */
        [[nodiscard]] std::string foreignKeyOnDelete() const;

        /**
         * Set foreign key reference (fluent interface).
         * @param table Reference table name
         * @param column Reference column name (defaults to "id")
         * @param onUpdate Update policy (defaults to "RESTRICT")
         * @param onDelete Delete policy (defaults to "RESTRICT")
         * @return Reference to self for chaining
         */
        EntitySchemaField &setForeignKey(const std::string &table, 
                                         const std::string &column = "id",
                                         const std::string &onUpdate = "RESTRICT",
                                         const std::string &onDelete = "RESTRICT");

        /**
         * Remove foreign key constraint (fluent interface).
         * @return Reference to self for chaining
         */
        EntitySchemaField &removeForeignKey();

        /**
         * Get all field constraints.
         * @return JSON object with constraints (min, max, validator, etc.)
         */
        [[nodiscard]] nlohmann::json constraints() const;

        /**
         * Get specific constraint by key.
         * @param key Constraint key (e.g., "min", "max", "validator")
         * @return Constraint value JSON
         */
        [[nodiscard]] nlohmann::json constraint(const std::string &key) const;

        /**
         * Set field constraints (fluent interface).
         * @param opts JSON object with constraint values
         * @return Reference to self for chaining
         */
        EntitySchemaField &setConstraints(const nlohmann::json &opts);

        // ----------------- SCHEMA FIELD OPS ---------------------- //
        /**
         * Update field with new JSON data.
         * @param field_schema JSON object with field updates
         * @return Reference to self for chaining
         */
        EntitySchemaField &updateWith(const nlohmann::json &field_schema);

        /**
         * Convert field to JSON representation.
         * @return JSON object with field definition
         */
        [[nodiscard]] nlohmann::json toJSON() const;

        /**
         * Convert field type to SOCI database type.
         * @return SOCI db_type enum value
         */
        [[nodiscard]] soci::db_type toSociType() const;

        /**
         * Convert type string to SOCI database type (static).
         * @param type Field type string
         * @return SOCI db_type enum value
         */
        [[nodiscard]] static soci::db_type toSociType(const std::string &type);

        /**
         * Validate field definition.
         * @return Optional error message if validation fails
         */
        [[nodiscard]] std::optional<std::string> validate() const;

        /**
         * Check if field type is valid.
         * @param type Type string to validate
         * @return true if valid type
         */
        static bool isValidFieldType(const std::string &type);

        /**
         * Generate unique field ID from name.
         * @param id Field name or identifier
         * @return Generated field ID string
         */
        static std::string genFieldId(const std::string &id);

        /**
         * Get default constraints JSON.
         * @return Reference to default constraints object
         */
        static const nlohmann::json &defaultConstraints();

    private:
        std::string m_id, m_name, m_type;
        bool m_required = false, m_primaryKey = false, m_isSystem = false, m_isUnique = false;
        nlohmann::json m_constraints{}, m_foreignKey{};
    };
} // mb

#endif //MANTISBASE_ENTITY_SCHEMA_FIELD_H
