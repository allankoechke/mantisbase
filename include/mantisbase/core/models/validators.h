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

    class Validators {
    public:
        static std::optional<json> findPreset(const std::string &key);

        static std::optional<std::string> validatePreset(const std::string &key, const std::string &value);

        static std::optional<std::string> minimumConstraintCheck(const json &field, const json &body);

        static std::optional<std::string> maximumConstraintCheck(const json &field, const json &body);

        static std::optional<std::string> requiredConstraintCheck(const json &field, const json &body);

        static std::optional<std::string> validatorConstraintCheck(const json &field, const json &body);

        static std::optional<std::string> viewTypeSQLCheck(const json &body);

        static std::optional<std::string> validateTableSchema(const json &entity_schema);

        static std::optional<std::string> validateRequestBody(const json &schema, const json &body);

        static std::optional<std::string> validateRequestBody(const Entity &schema, const json &body);

        static std::optional<std::string> validateUpdateRequestBody(const json &schema, const json &body);

        static std::optional<std::string> validateUpdateRequestBody(const Entity &schema, const json &body);

    private:
        static std::unordered_map<std::string, json> presets;
    };
} // mb

#endif //MANTISBASE_VALIDATORS_H
