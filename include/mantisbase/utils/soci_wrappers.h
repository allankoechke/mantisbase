/**
 * @file soci_wrappers.h
 * @brief SOCI database wrapper utilities.
 *
 * Provides conversion functions between JSON objects and SOCI database
 * value types for seamless data binding and retrieval.
 */

#ifndef MANTISAPP_SOCI_WRAPPERS_H
#define MANTISAPP_SOCI_WRAPPERS_H

#include "utils.h"
#include "../mantisbase.h"
#include "mantisbase/core/models/entity_schema_field.h"
#include "soci/values.h"

namespace mb {
    inline soci::values json2SociValue(const json &entity, const json &fields) {
        if (!fields.is_array()) throw std::invalid_argument("Fields must be an array");

        soci::values vals;
        // Bind parameters dynamically
        for (const auto &field: fields) {
            const auto field_name = field.at("name").get<std::string>();

            if (field_name == "id" || field_name == "created" || field_name == "updated") {
                continue;
            }

            // For password types, let's hash them before binding to DB
            if (field_name == "password") {
                // Extract password value and hash it
                auto hashed_pswd = hashPassword(entity.at(field_name).get<std::string>());
                // Add the hashed password to the soci::vals
                vals.set(field_name, hashed_pswd);
            }
            else {
                // Skip fields that are not in the json object
                if (!entity.contains(field_name)) continue;

                // If the value is null, set i_null and continue
                if (entity[field_name].is_null()) {
                    std::optional<int> val; // Set to optional, no value is set in db
                    vals.set(field_name, val, soci::i_null);
                    continue;
                }

                // For non-null values, set the value accordingly
                const auto field_type = field.at("type").get<std::string>();
                if (field_type == "xml" || field_type == "string" || field_type == "file") {
                    vals.set(field_name, entity.value(field_name, ""));
                } else if (field_type == "double") {
                    vals.set(field_name, entity.value(field_name, 0.0));
                } else if (field_type == "date") {
                    auto dt_str = entity.value(field_name, "");
                    if (dt_str.empty()) {
                        vals.set(field_name, 0, soci::i_null);
                    } else {
                        std::tm tm{};
                        std::istringstream ss{dt_str};
                        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

                        vals.set(field_name, tm);
                    }
                } else if (field_type == "int8") {
                    vals.set(field_name, static_cast<int8_t>(entity.value(field_name, 0)));
                } else if (field_type == "uint8") {
                    vals.set(field_name, static_cast<uint8_t>(entity.value(field_name, 0)));
                } else if (field_type == "int16") {
                    vals.set(field_name, static_cast<int16_t>(entity.value(field_name, 0)));
                } else if (field_type == "uint16") {
                    vals.set(field_name, static_cast<uint16_t>(entity.value(field_name, 0)));
                } else if (field_type == "int32") {
                    vals.set(field_name, static_cast<int32_t>(entity.value(field_name, 0)));
                } else if (field_type == "uint32") {
                    vals.set(field_name, static_cast<uint32_t>(entity.value(field_name, 0)));
                } else if (field_type == "int64") {
                    vals.set(field_name, static_cast<int64_t>(entity.value(field_name, 0)));
                } else if (field_type == "uint64") {
                    vals.set(field_name, static_cast<uint64_t>(entity.value(field_name, 0)));
                } else if (field_type == "blob") {
                    // TODO implement BLOB type
                    // vals.set(field_name, entity.value(field_name, sql->empty_blob()));
                } else if (field_type == "json") {
                    vals.set(field_name, entity.value(field_name, json::object()));
                } else if (field_type == "bool") {
                    vals.set(field_name, entity.value(field_name, false));
                } else if (field_type == "files") {
                    vals.set(field_name, entity.value(field_name, json::array()));
                }
            }
        }

        return vals;
    }

    inline std::string getColumnType(const std::string& column_name, const std::vector<json>& fields)
    {
        if (column_name.empty()) throw std::invalid_argument("Column name can't be empty!");

        for (const auto& field : fields)
        {
            if (field.value("name", "") == column_name)
                return field.at("type").get<std::string>();
        }

        throw std::runtime_error("No field type found matching column `" + column_name + "'");
    }

    inline json sociRow2Json(const soci::row& row, const std::vector<json>& entity_fields)
    {
        // Guard against empty reference schema fields
        if (entity_fields.empty())
            throw std::invalid_argument("Reference schema fields can't be empty!");

        // Build response json object
        json res_json;
        for (size_t i = 0; i < row.size(); i++)
        {
            const auto colName = row.get_properties(i).get_name();
            const auto colType = getColumnType(colName, entity_fields);

            // Check column type is valid type
            if (colType.empty() || !EntitySchemaField::isValidFieldType(colType)) // Or not in expected types
            {
                // Throw an error for unknown types
                throw std::runtime_error(std::format("Unknown column type `{}` for column `{}`", colType, colName));
            }

            // Handle null values immediately
            if (row.get_indicator(i) == soci::i_null)
            {
                // Handle null value in JSON
                res_json[colName] = nullptr;
                continue;
            }

            // Handle type conversions
            if (colType == "xml" || colType == "string")
            {
                res_json[colName] = row.get<std::string>(i, "");
            }
            else if (colType == "double")
            {
                res_json[colName] = row.get<double>(i);
            }
            else if (colType == "date")
            {
                res_json[colName] = mb::dbDateToString(row, i);
            }
            else if (colType == "int8")
            {
                res_json[colName] = row.get<int8_t>(i);
            }
            else if (colType == "uint8")
            {
                res_json[colName] = row.get<uint8_t>(i);
            }
            else if (colType == "int16")
            {
                res_json[colName] = row.get<int16_t>(i);
            }
            else if (colType == "uint16")
            {
                res_json[colName] = row.get<uint16_t>(i);
            }
            else if (colType == "int32")
            {
                res_json[colName] = row.get<int32_t>(i);
            }
            else if (colType == "uint32")
            {
                res_json[colName] = row.get<uint32_t>(i);
            }
            else if (colType == "int64")
            {
                res_json[colName] = row.get<int64_t>(i);
            }
            else if (colType == "uint64")
            {
                res_json[colName] = row.get<uint64_t>(i);
            }
            else if (colType == "blob")
            {
                // TODO ? How do we handle BLOB?
                // j[colName] = row.get<std::string>(i);
            }
            else if (colType == "json" || colType == "list")
            {
                res_json[colName] = row.get<json>(i);
            }
            else if (colType == "bool")
            {
                res_json[colName] = row.get<bool>(i);
            }
            else if (colType == "file")
            {
                res_json[colName] = row.get<std::string>(i);
            }
            else if (colType == "files")
            {
                res_json[colName] = row.get<json>(i);
            }
        }

        return res_json;
    }

}

#endif //MANTISAPP_SOCI_WRAPPERS_H
