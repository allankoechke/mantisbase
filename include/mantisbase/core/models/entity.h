/**
 * @file entity.h
 * @brief Entity class for database table operations and CRUD functionality.
 *
 * Represents a database table/entity with schema information and provides
 * methods for creating, reading, updating, and deleting records.
 */

#ifndef MANTISBASE_ENTITY_H
#define MANTISBASE_ENTITY_H

#include <string>
#include "mantisbase/mantis.h"
#include "mantisbase/core/exceptions.h"
#include "mantisbase/utils/soci_wrappers.h"
#include "../types.h"
#include "access_rules.h"

namespace mb {
    using Record = nlohmann::json;  ///< Single database record as JSON object
    using Records = std::vector<Record>;  ///< Collection of database records

    /**
     * @brief Represents a database table/entity with schema and CRUD operations.
     *
     * Entity provides methods to interact with database tables including
     * creating, reading, updating, and deleting records. It encapsulates
     * schema information and access rules.
     *
     * @code
     * // Create entity from schema JSON
     * json schema = {{"name", "users"}, {"type", "base"}, {"fields", ...}};
     * Entity entity(schema);
     *
     * // Create entity with name and type
     * Entity entity("posts", "base");
     * @endcode
     */
    class Entity {
    public:
        /**
         * @brief Construct entity from schema JSON object.
         * @param schema JSON object containing table schema (name, type, fields, rules, etc.)
         */
        explicit Entity(const nlohmann::json &schema);
        
        /**
         * @brief Construct entity with name and type.
         * @param name Table name
         * @param type Entity type ("base", "auth", or "view")
         */
        explicit Entity(const std::string &name, const std::string& type);

        // --------------- DB TABLE OPS ------------------ //
        /**
         * @brief Get entity unique identifier.
         * @return Entity ID string
         */
        [[nodiscard]] std::string id() const;

        /**
         * @brief Get entity table name.
         * @return Table name
         */
        [[nodiscard]] std::string name() const;

        /**
         * @brief Get entity type ("base", "auth", or "view").
         * @return Entity type string
         */
        [[nodiscard]] std::string type() const;

        /**
         * @brief Check if entity is a system table.
         * @return true if system table, false otherwise
         */
        [[nodiscard]] bool isSystem() const;

        /**
         * @brief Check if entity has API endpoints enabled.
         * @return true if API enabled, false otherwise
         */
        [[nodiscard]] bool hasApi() const;

        /**
         * @brief Get SQL query for view type entities.
         * @return View SQL query string
         * @throws MantisException if entity is not a view type
         */
        [[nodiscard]] std::string viewQuery() const;

        /**
         * @brief Get all field definitions.
         * @return Reference to vector of field JSON objects
         */
        [[nodiscard]] const std::vector<json> &fields() const;
        
        /**
         * @brief Get field definition by name.
         * @param field_name Field name to lookup
         * @return Optional field JSON object if found
         */
        [[nodiscard]] std::optional<json> field(const std::string& field_name) const;
        
        /**
         * @brief Check if field exists by name.
         * @param field_name Field name to check
         * @return Optional field JSON if exists, nullopt otherwise
         */
        [[nodiscard]] std::optional<json> hasField(const std::string& field_name) const;

        /**
         * @brief Get all access rules.
         * @return Reference to rules JSON object
         */
        [[nodiscard]] const json& rules() const;

        /**
         * @brief Get list access rule (for GET /api/v1/entities/{table}).
         * @return AccessRule for listing records
         */
        [[nodiscard]] AccessRule listRule() const;

        /**
         * @brief Get read access rule (for GET /api/v1/entities/{table}/:id).
         * @return AccessRule for reading a record
         */
        [[nodiscard]] AccessRule getRule() const;

        /**
         * @brief Get create access rule (for POST /api/v1/entities/{table}).
         * @return AccessRule for creating records
         */
        [[nodiscard]] AccessRule addRule() const;

        /**
         * @brief Get update access rule (for PATCH /api/v1/entities/{table}/:id).
         * @return AccessRule for updating records
         */
        [[nodiscard]] AccessRule updateRule() const;

        /**
         * @brief Get delete access rule (for DELETE /api/v1/entities/{table}/:id).
         * @return AccessRule for deleting records
         */
        [[nodiscard]] AccessRule deleteRule() const;

        // --------------- DB CRUD OPS ------------------ //
        /**
         * @brief Create a new record in the entity table.
         * @param record JSON object with field values
         * @param opts Optional parameters (currently unused)
         * @return Created record with generated ID and timestamps
         * @code
         * json newUser = {{"name", "John"}, {"email", "john@example.com"}};
         * Record created = entity.create(newUser);
         * @endcode
         */
        [[nodiscard]] Record create(const json &record, const json &opts = json::object()) const;

        /**
         * @brief List all records in the entity table.
         * @param opts Optional parameters (currently unused)
         * @return Vector of record JSON objects
         */
        [[nodiscard]] Records list(const json &opts = json::object()) const;

        /**
         * @brief Read a single record by ID.
         * @param id Record identifier
         * @param opts Optional parameters (currently unused)
         * @return Optional record JSON if found, nullopt otherwise
         */
        [[nodiscard]] std::optional<Record> read(const std::string &id, const json &opts = json::object()) const;

        /**
         * @brief Update an existing record by ID.
         * @param id Record identifier
         * @param data JSON object with fields to update
         * @param opts Optional parameters (currently unused)
         * @return Updated record JSON
         */
        [[nodiscard]] Record update(const std::string &id, const json &data, const json &opts = json::object()) const;

        /**
         * @brief Delete a record by ID.
         * @param id Record identifier to delete
         */
        void remove(const std::string &id) const;

        // --------------- SCHEMA OPS ------------------ //
        /**
         * @brief Get complete entity schema JSON.
         * @return Reference to schema JSON object
         */
        [[nodiscard]] const json &schema() const;

        /**
         * @brief Count total records in the entity table.
         * @return Number of records
         */
        [[nodiscard]] int countRecords() const;

        /**
         * @brief Check if entity table is empty.
         * @return true if no records exist, false otherwise
         */
        [[nodiscard]] bool isEmpty() const;

        // --------------- UTILITY OPS ------------------ //
        /**
         * @brief Check if a record exists by ID.
         * @param id Record identifier to check
         * @return true if record exists, false otherwise
         */
        [[nodiscard]] bool recordExists(const std::string &id) const;

        /**
         * @brief Find field definition by name (alias for field()).
         * @param field_name Field name to find
         * @return Optional field JSON if found
         */
        [[nodiscard]] std::optional<json> findField(const std::string &field_name) const;

        /**
         * @brief Query records by searching across multiple columns.
         * @param value Value to search for
         * @param columns Column names to search in
         * @return Optional record JSON if found
         */
        [[nodiscard]] std::optional<json> queryFromCols(const std::string &value,
                                                        const std::vector<std::string> &columns) const;

        // --------------- SCHEMA ROUTING ------------------ //
        /**
         * @brief Get route handler for GET /api/v1/entities/{table}/:id.
         * @return Handler function for single record retrieval
         */
        [[nodiscard]] HandlerFn getOneRouteHandler() const;

        /**
         * @brief Get route handler for GET /api/v1/entities/{table}.
         * @return Handler function for listing records
         */
        [[nodiscard]] HandlerFn getManyRouteHandler() const;

        /**
         * @brief Get route handler for POST /api/v1/entities/{table}.
         * @return Handler function with content reader for record creation
         */
        [[nodiscard]] HandlerWithContentReaderFn postRouteHandler() const;

        /**
         * @brief Get route handler for PATCH /api/v1/entities/{table}/:id.
         * @return Handler function with content reader for record updates
         */
        [[nodiscard]] HandlerWithContentReaderFn patchRouteHandler() const;

        /**
         * @brief Get route handler for DELETE /api/v1/entities/{table}/:id.
         * @return Handler function for record deletion
         */
        [[nodiscard]] HandlerFn deleteRouteHandler() const;

        /**
         * @brief Register all CRUD routes for this entity with the router.
         */
        void createEntityRoutes() const;

    private:
        nlohmann::json m_schema;
    };
} // mb

#endif //MANTISBASE_ENTITY_H
