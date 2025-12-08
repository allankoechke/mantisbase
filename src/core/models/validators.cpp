//
// Created by codeart on 11/11/2025.
//

#include "../../../include/mantisbase/core/models/validators.h"

namespace mantis {
    std::unordered_map<std::string, json> Validators::presets = {
        {
            "email", json{
                {"regex", R"(^[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}$)"},
                {"error", "Email format is not valid"}
            }
        },
        {
            "password", json{
                {"regex", R"(^\S{8,}$)"},
                {"error", "Expected 8 chars minimum with no whitespaces."}
            }
        },
        {
            "password-long", json{
                {"regex", R"(^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)(?=.*[\W_]).{8,}$)"},
                {
                    "error",
                    "Expected at least one lowercase, uppercase, digit, special character, and a min 8 chars."
                }
            }
        }
    };

    std::optional<json> Validators::findPreset(const std::string &key) {
        if (key.empty()) return std::nullopt;

        const auto n_key = key.starts_with("@") ? key.substr(1) : key;
        if (const auto it = presets.find(n_key); it != presets.end()) {
            return it->second;
        }

        return std::nullopt;
    }

    std::optional<std::string> Validators::validatePreset(const std::string &key, const std::string &value) {
        if (key.empty())
            throw MantisException(500, "Validator key can't be empty!");

        const auto v = findPreset(key).value_or(json::object());
        if (v.empty())
            throw MantisException(500, "Validator key is not available!");

        // Since we have a regex string, lets validate it and return if it fails ...
        const auto &reg = v["regex"].get<std::string>();
        const auto &err = v["error"].get<std::string>();

        if (const std::regex r_pattern(reg); !std::regex_match(value, r_pattern)) {
            throw MantisException(500, err);
        }

        // If we reach here, then, all was validated correctly!
        return std::nullopt;
    }

    std::optional<std::string> Validators::minimumConstraintCheck(const json &field, const json &body) {
        if (!field["constraints"]["min_value"].is_null()) {
            const auto &min_value = field["constraints"]["min_value"].get<double>();
            const auto &field_name = field["name"].get<std::string>();
            const auto &field_type = field["name"].get<std::string>();

            if (field_type == "string" && body.value(field_name, "").size() < static_cast<size_t>(min_value)) {
                return
                        std::format("Minimum Constraint Failed: Char length for `{}` should be >= {}",
                                    field_name, static_cast<int>(min_value));
            }

            if (field_type == "double" || field_type == "int8" || field_type == "uint8" || field_type == "int16" ||
                field_type == "uint16" || field_type == "int32" || field_type == "uint32" || field_type == "int64" ||
                field_type == "uint64") {
                if (body.at(field_name) < min_value) {
                    return
                            std::format("Minimum Constraint Failed: Value for `{}` should be >= {}",
                                        field_name, min_value);
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> Validators::maximumConstraintCheck(const json &field, const json &body) {
        if (!field["constraints"]["max_value"].is_null()) {
            const auto max_value = field["constraints"]["max_value"].get<double>();
            const auto &field_name = field["name"].get<std::string>();
            const auto &field_type = field["name"].get<std::string>();

            if (field_type == "string" && body.value(field_name, "").size() > static_cast<size_t>(max_value)) {
                return
                        std::format("Minimum Constraint Failed: Char length for `{}` should be >= {}",
                                    field_name, static_cast<int>(max_value));
            }

            if (field_type == "double" || field_type == "int8" || field_type == "uint8" || field_type == "int16" ||
                field_type == "uint16" || field_type == "int32" || field_type == "uint32" || field_type == "int64" ||
                field_type == "uint64") {
                if (body.at(field_name) > max_value) {
                    return
                            std::format("Maximum Constraint Failed: Value for `{}` should be <= {}",
                                        field_name, max_value);
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> Validators::requiredConstraintCheck(const json &field, const json &body) {
        // Get the required flag and the field name
        const auto &required = field.value("required", false);
        const auto &field_name = field["name"].get<std::string>();
        const bool has_default_value = !field["constraints"]["default_value"].is_null();

        // If default value is set on db level and value is null, skip this check
        if (required && !has_default_value && body[field_name].is_null()) {
            return std::format("Field `{}` is required", field_name);
        }

        return std::nullopt;
    }

    std::optional<std::string> Validators::validatorConstraintCheck(const json &field, const json &body) {
        if (field["constraints"]["validator"].is_string()) {
            const auto &pattern = field["constraints"]["validator"].get<std::string>();
            const auto &field_name = field["name"].get<std::string>();
            const auto &field_type = field["type"].get<std::string>();

            // Check if we have a regex or typed validator from our store
            const auto opt = Validators::findPreset(pattern);
            if (opt.has_value() && field_type == "string") {
                // Since we have a regex string, lets validate it and return if it fails ...
                const auto &reg = opt.value()["regex"].get<std::string>();
                const auto &err = opt.value()["error"].get<std::string>();

                auto f = body.at(field_name).get<std::string>();
                if (const std::regex r_pattern(reg); !std::regex_match(f, r_pattern)) {
                    return std::nullopt;
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> Validators::viewTypeSQLCheck(const json &body) {
        if (!body.contains("view_query") || body["view_query"].is_null() || trim(body["view_query"].get<std::string>()).empty()) {
            return "View tables require a valid SQL View query!";
        }

        // TODO
        // const auto& sql = trim(entity.at("sql").get<std::string>());
        // Check that it contains an `id` field
        // Check it does not contain malicious things!!

        return std::nullopt;
    }

    std::optional<std::string> Validators::validateTableSchema(const json &entity_schema) {
        if (!entity_schema.contains("name")
            || entity_schema["name"].is_null()
            || trim(entity_schema.value("name", "")).empty()) {
            return "Table schema is missing a valid `name` field";
        }

        if (!entity_schema.contains("type") || entity_schema["type"].is_null()) {
            return "Table type is missing. Expected `base`, `view`, or `auth`.";
        }

        // Check that the type is either a view|base|auth type
        auto type = trim(entity_schema.at("type").get<std::string>());
        toLowerCase(type);
        if (!(type == "view" || type == "base" || type == "auth")) {
            return "Table type should be either `base`, `view`, or `auth`.";
        }

        // If the table type is of view type, check that the SQL is passed in ...
        if (type == "view") {
            if (const auto err = viewTypeSQLCheck(entity_schema); err.has_value())
                return err.value();
        } else if (entity_schema.contains("fields") && !entity_schema["fields"].is_null()) {
            // Check fields if any is added
            for (const auto &field: entity_schema.value("fields", std::vector<json>())) {
                if (!field.contains("name") || field["name"].is_null() || trim(field["name"].get<std::string>()).
                    empty()) {
                    return "One of the fields is missing a valid name";
                }

                if (!field.contains("type") || field["type"].is_null() || trim(field["type"].get<std::string>()).
                    empty()) {
                    return std::format("Field type `{}` for `{}` is empty!",
                                       field["type"].get<std::string>(), field["name"].get<std::string>());
                }

                if (!EntitySchemaField::isValidFieldType(trim(field["type"].get<std::string>()))) {
                    return std::format("Field type `{}` for `{}` is not recognized!",
                                       field["type"].get<std::string>(), field["name"].get<std::string>());
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> Validators::validateRequestBody(const json &schema, const json &body) {
        const auto entity_obj = Entity{schema};
        return validateRequestBody(entity_obj, body);
    }

    std::optional<std::string> Validators::validateRequestBody(const Entity &entity, const json &body) {
        // If the table type is of view type, check that the SQL is passed in ...
        if (entity.type() == "view") {
            if (const auto err = viewTypeSQLCheck(body); err.has_value()) return err;
        } else // For `base` and `auth` types
        {
            // Create default base object
            for (const auto &field: entity.fields()) {
                // Skip system generated fields
                if (const auto &name = field["name"].get<std::string>();
                    name == "id" || name == "created" || name == "updated")
                    continue;

                // REQUIRED CONSTRAINT CHECK
                if (const auto err = requiredConstraintCheck(field, body); err.has_value()) return err;

                // MINIMUM CONSTRAINT CHECK
                if (const auto err = minimumConstraintCheck(field, body); err.has_value()) return err;

                // MAXIMUM CONSTRAINT CHECK
                if (const auto err = maximumConstraintCheck(field, body); err.has_value()) return err;

                // VALIDATOR CONSTRAINT CHECK
                if (const auto err = validatorConstraintCheck(field, body); err.has_value()) return err;
            }
        }

        // Return null option for no error cases
        return std::nullopt;
    }

    std::optional<std::string> Validators::validateUpdateRequestBody(const Entity &entity, const json &body) {
        // If the table type is of view type, check that the SQL is passed in ...
        if (entity.type() == "view") {
            if (const auto err = viewTypeSQLCheck(body); err.has_value()) return err;
        } else // For `base` and `auth` types
        {
            // Create default base object
            for (const auto &[key, val]: body.items()) {
                if (!entity.hasField(key)) {
                    return std::format("Unknown field named `{}`!", key);
                }

                // Get field data
                const auto field = entity.field(key).value();

                // Skip system generated fields
                if (const auto &name = field["name"].get<std::string>();
                    name == "id" || name == "created" || name == "updated")
                    continue;

                // REQUIRED CONSTRAINT CHECK
                if (const auto err = requiredConstraintCheck(field, body); err.has_value()) return err;

                // MINIMUM CONSTRAINT CHECK
                if (const auto err = minimumConstraintCheck(field, body); err.has_value()) return err;

                // MAXIMUM CONSTRAINT CHECK
                if (const auto err = maximumConstraintCheck(field, body); err.has_value()) return err;

                // VALIDATOR CONSTRAINT CHECK
                if (const auto err = validatorConstraintCheck(field, body); err.has_value()) return err;
            }
        }

        // Return null option for no error cases
        return std::nullopt;
    }

    std::optional<std::string> Validators::validateUpdateRequestBody(const json &schema, const json &body) {
        const auto entity_obj = Entity{schema};
        return validateUpdateRequestBody(entity_obj, body);
    }
}
