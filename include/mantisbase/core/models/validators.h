/**
 * @file validators.h
 * @brief Validation utilities for entity schemas and request bodies.
 *
 * Provides validation methods for checking field constraints, presets,
 * and request body validation against entity schemas.
 */

#ifndef MANTISBASE_VALIDATORS_H
#define MANTISBASE_VALIDATORS_H

#include <string>

#include "mantisbase/core/database.h"
#include "mantisbase/core/models/entity.h"
#include "nlohmann/json.hpp"


namespace mb {
    using json = nlohmann::json;

    /**
     * @brief Static validators for schema definitions and CRUD request bodies.
     *
     * Each check returns `std::nullopt` on success or an error message string.
     */
    class Validators {
    public:
        /** Look up a built-in validator preset definition by key. */
        static std::optional<json> findPreset(const std::string &key);

        /** Validate a value against a named preset (email, url, etc.). */
        static std::optional<std::string> validatePreset(const std::string &key, const std::string &value);

        /** Enforce optional `min` constraint on numeric/string fields. */
        static std::optional<std::string> minimumConstraintCheck(const json &field, const json &body);

        /** Enforce optional `max` constraint on numeric/string fields. */
        static std::optional<std::string> maximumConstraintCheck(const json &field, const json &body);

        /** Enforce int precision and range for int-typed fields. */
        static std::optional<std::string> intPrecisionValueCheck(const json &field, const json &body);

        /** Enforce `required` when the field is marked required in the schema. */
        static std::optional<std::string> requiredConstraintCheck(const json &field, const json &body);

        /** Run custom validator preset named in the field constraints. */
        static std::optional<std::string> validatorConstraintCheck(const json &field, const json &body);

        /**
         * @brief Validate a foreign-key field against a referenced entity record.
         *
         * Requires a @ref MantisBase reference so the check can load the target
         * entity through `app.entity(...)` instead of a global accessor.
         */
        static std::optional<std::string> foreignKeyConstraintCheck(const MantisBase& app, const json &field, const json &body);

        /** Validate `viewQuery` SQL for view-type entity schemas. */
        static std::optional<std::string> viewTypeSQLCheck(const json &body);

        /** Validate a full entity schema JSON before create/update. */
        static std::optional<std::string> validateTableSchema(const json &entity_schema);

        /** Validate a create (POST) body against an entity schema. */
        static std::optional<std::string> validateRequestBody(const Entity &entity, const json &body);

        /** Validate a partial update (PATCH) body against an entity schema. */
        static std::optional<std::string> validateUpdateRequestBody(const Entity &entity, const json &body);

    private:
        static std::unordered_map<std::string, json> presets;
    };
} // mb

#endif //MANTISBASE_VALIDATORS_H
