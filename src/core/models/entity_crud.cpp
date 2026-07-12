#include "../../../include/mantisbase/core/models/entity.h"
#include "../../../include/mantisbase/core/models/entity_schema.h"
#include "../../../include/mantisbase/core/models/entity_schema_field.h"
#include "../../../include/mantisbase/utils/utils.h"
#include "../../../include/mantisbase/utils/uuidv7.h"
#include "mantisbase/utils/soci_wrappers.h"
#include "mantisbase/core/realtime.h"


namespace mb {
    // --------------------------------------------------------------------------- //
    // CRUD OPS                                                                    //
    // --------------------------------------------------------------------------- //

    Record Entity::create(const json &record, const json &opts) const {
        // Database session & transaction instance
        auto sql = app().db().session();
        soci::transaction tr(*sql);

        try {
            // Create a random ID, then check if it exists already in the DB
            std::string id = generate_uuidv7();
            int trials = 1;
            while (recordExists(id)) {
                // Try getting a new ID ten times before giving up. Avoid infinite loops
                if (trials >= 10)
                    break;

                id = generate_uuidv7();
                trials++;
            }

            // Create default time values
            std::time_t current_t = time(nullptr);
            std::tm created_tm = toUtcTime(current_t);
            std::string columns, placeholders;

            // Clone record for editing ...
            json new_record = record;

            // Force default values here ...
            new_record["id"] = "";
            new_record["created"] = nullptr;
            new_record["updated"] = nullptr;

            columns += "id";
            placeholders += ":id";
            columns += ", created";
            placeholders += ", :created";
            columns += ", updated";
            placeholders += ", :updated";

            // Create the field cols and value cols as concatenated strings
            for (const auto &[field_name, _]: record.items()) {
                // First, ensure the key exists in our schema fields
                if (auto field_schema = findField(field_name); !field_schema.has_value()) continue;

                new_record[field_name] = record[field_name];
                const auto col = sqlIdentifier(field_name);
                columns += columns.empty() ? col : ", " + col;
                placeholders += placeholders.empty() ? (":" + col) : (", :" + col);
            }

            // Create the SQL Query. RETURNING * lets us read the freshly stored
            // row back in the same round-trip instead of issuing a separate
            // SELECT (supported by SQLite >= 3.35 and PostgreSQL).
            std::string sql_query = std::format("INSERT INTO {} ({}) VALUES ({}) RETURNING *",
                                                sqlIdentifier(name()), columns, placeholders);

            // Bind soci::values to entity values
            auto vals = json2SociValue(new_record, fields());
            vals.set("id", id, soci::i_ok);
            vals.set("created", created_tm, soci::i_ok);
            vals.set("updated", created_tm, soci::i_ok);

            // Execute the insert and fetch the newly-created row in one go
            soci::row r;
            *sql << sql_query, soci::use(vals), soci::into(r);
            tr.commit();

            // Wake the realtime worker to deliver the change immediately.
            app().rt().notifyChange();

            auto added_row = sociRow2Json(r, fields());

            // Remove user password from the response
            if (type() == "auth") added_row.erase("password");
            return added_row;
        } catch (...) {
            // Handles anything else
            // logEntry::critical("Error executing Entity::create()");
            throw; // rethrow unknown exception
        }
    }

    Records Entity::list(const json &opts) const {
        const auto sql = MantisBase::instance().db().session();
        int limit = 50;
        std::string after;
        std::string sort_field = "id";
        std::string sort_dir = "ASC";

        if (opts.contains("pagination") && opts["pagination"].is_object()) {
            auto &pagination = opts["pagination"];

            if (pagination.contains("limit") && pagination["limit"].is_number()) {
                limit = pagination["limit"].get<int>();
                if (limit < 1) limit = 1;
                if (limit > 500) limit = 500;
            }
            if (pagination.contains("after") && pagination["after"].is_string())
                after = pagination["after"].get<std::string>();
            if (pagination.contains("sort") && pagination["sort"].is_string()) {
                auto sort_str = pagination["sort"].get<std::string>();
                if (!sort_str.empty() && sort_str[0] == '-') {
                    sort_dir = "DESC";
                    sort_field = sort_str.substr(1);
                } else {
                    sort_field = sort_str;
                }
                bool valid = false;
                for (const auto &f : fields()) {
                    if (f.contains("name") && f["name"].get<std::string>() == sort_field) {
                        valid = true;
                        break;
                    }
                }
                if (!valid) {
                    sort_field = "id";
                    sort_dir = "ASC";
                }
            }
        }

        std::string query = "SELECT * FROM " + name();
        if (!after.empty()) {
            if (sort_dir == "ASC")
                query += " WHERE " + sort_field + " > :after";
            else
                query += " WHERE " + sort_field + " < :after";
        }
        query += " ORDER BY " + sort_field + " " + sort_dir + " LIMIT :limit";

        soci::rowset<soci::row> rs = after.empty()
            ? (sql->prepare << query, soci::use(limit))
            : (sql->prepare << query, soci::use(after), soci::use(limit));

        nlohmann::json record_list = nlohmann::json::array();

        for (const auto &row: rs) {
            auto row_json = sociRow2Json(row, fields());
            if (type() == "auth") {
                row_json.erase("password");
            }
            record_list.push_back(row_json);
        }

        return record_list;
    }

    std::optional<Record> Entity::read(const std::string &id, const json &opts) const {
        // Get a soci::session from the pool
        const auto sql = app().db().session();

        soci::row r; // To hold read data
        *sql << std::format("SELECT * FROM {} WHERE id = :id", sqlIdentifier(name())), soci::use(id), soci::into(r);

        // If no data was found, return a std::nullopt
        if (!sql->got_data()) return std::nullopt; // 404

        // Parse returned record to JSON
        auto record = sociRow2Json(r, fields());

        if (opts.contains("keep_passwords") &&
            opts["keep_passwords"].is_boolean() &&
            opts["keep_passwords"].get<bool>())
            return record; // Skip redacting passwords

        // Remove user password from the response
        if (type() == "auth") record.erase("password");

        // Return the record
        return record;
    }

    Record Entity::update(const std::string &id, const json &data, const json &opts) const {
        // Database session & transaction instance
        auto sql = app().db().session();
        soci::transaction tr(*sql);

        try {
            // Create default time values
            std::time_t current_t = time(nullptr);
            std::tm created_tm = toUtcTime(current_t);
            std::string columns, placeholders;

            // Store files to delete by filename
            std::vector<std::string> files_to_delete{};
            std::vector<json> file_fields{};

            // Create a temporary container to track fields we intend to update.
            // Why? We'll limit to the fields we have in our schema, that way, we
            // don't have any surprises.
            std::vector<std::string> updateFields;
            updateFields.reserve(data.size());

            // Create the field cols and value cols as concatenated strings
            for (const auto &[key, val]: data.items()) {
                // For system fields, let's ignore them for now.
                if (key == "id" || key == "created" || key == "updated") continue;

                // First, ensure the key exists in our schema fields
                auto schema = findField(key);
                if (!schema.has_value()) continue;

                const auto col = sqlIdentifier(key);
                columns += columns.empty() ? std::format("{0} = :{0}", col) : std::format(", {0} = :{0}", col);
                updateFields.push_back(key);

                // Track file fields for use later on
                if (schema.value()["type"] == "file" || schema.value()["type"] == "files") {
                    file_fields.push_back(
                        json{
                            {"name", key},
                            {"value", val},
                            {
                                "type", schema.value()["type"]
                            }
                        });
                }
            }

            if (columns.empty())
                throw MantisException(
                    400, data.empty()
                             ? "No column data provided for updating."
                             : "No matching column in schema to update."
                );

            // Add Updated field as an extra field for updates ...
            columns += columns.empty() ? ("updated = :updated") : (", updated = :updated");
            updateFields.emplace_back("updated");

            // Check for file(s) being saved from the request, determine if there is
            // need to delete/overwrite existing files
            if (!file_fields.empty()) {
                std::string fields_to_query{};

                for (const auto &file: file_fields) {
                    const auto col = sqlIdentifier(file["name"].get<std::string>());
                    if (fields_to_query.empty()) fields_to_query = col;
                    else fields_to_query += ", " + col;
                }

                const std::string sql_str = std::format("SELECT {} FROM {} WHERE id = :id LIMIT 1",
                                                        fields_to_query, sqlIdentifier(name()));

                soci::row r;
                *sql << sql_str, soci::use(id), soci::into(r);

                if (!sql->got_data()) {
                    throw std::runtime_error(std::format("Could not find record with id = {}", id));
                }

                // Parse soci::row to JSON object
                auto record = sociRow2Json(r, fields());

                // From the record, check for changes in files
                // Assuming record order is maintained on query ...
                for (const auto &file_field: file_fields) {
                    const auto field_name = file_field["name"].get<std::string>();

                    // For null values in db, continue
                    if (record[field_name].is_null()) continue;

                    const auto files_in_db = file_field["type"] == "files"
                                                 ? record[field_name]
                                                 : json::array({record[field_name]});

                    if (file_field["value"] == nullptr ||
                        (file_field["value"].is_array() && file_field["value"].empty()) ||
                        (file_field["value"].is_string() && file_field["value"].empty())) {
                        // If value set is null, add all file(s) to delete array
                        files_to_delete.insert(files_to_delete.end(), files_in_db.begin(), files_in_db.end());
                        continue;
                    }

                    const auto new_files = file_field["type"] == "files"
                                               ? file_field["value"]
                                               : json::array({file_field["value"]});

                    for (const auto &file: files_in_db) {
                        if (std::ranges::find(new_files, file) == new_files.end()) {
                            // The new list/file is missing the file named in the db, so delete it
                            files_to_delete.push_back(file);
                        }
                    }
                }
            }

            // Create the SQL Query. RETURNING * reads the updated row back in
            // the same round-trip instead of a separate SELECT (supported by
            // SQLite >= 3.35 and PostgreSQL).
            std::string sql_query = std::format("UPDATE {} SET {} WHERE id = :old_id RETURNING *",
                                                sqlIdentifier(name()), columns);

            // Bind soci::values to entity values, throws an error if it fails
            auto vals = json2SociValue(data, fields());
            vals.set("old_id", id);
            vals.set("updated", created_tm);

            // Bind values, execute, and fetch the updated row in one go
            soci::row r;
            *sql << sql_query, soci::use(vals), soci::into(r);
            tr.commit();

            // Wake the realtime worker to deliver the change immediately.
            app().rt().notifyChange();

            // Delete files, if any were removed ...
            Files::removeFiles(name(), files_to_delete);

            Record new_record = sociRow2Json(r, fields());

            // Redact passwords
            if (type() == "auth") new_record.erase("password");
            return new_record;
        } catch (const MantisException &) {
            throw;
        } catch (const std::exception &e) {
            throw MantisException(500, e.what());
        }
    }

    void Entity::remove(const std::string &id) const {
        // Views should not reach here
        if (type() == "view")
            throw std::invalid_argument("Remove is not implemented for Entity of `view` type!");

        const auto sql = app().db().session();
        soci::transaction tr(*sql);

        // Check if item exists of given id
        soci::row row;
        const std::string sqlStr = std::format("SELECT * FROM {} WHERE id = :id LIMIT 1", sqlIdentifier(name()));
        *sql << sqlStr, soci::use(id), soci::into(row);

        if (!sql->got_data()) {
            throw MantisException(404, std::format("Resource not found for given id `{}`", id));
        }

        // Remove from DB
        *sql << std::format("DELETE FROM {} WHERE id = :id", sqlIdentifier(name())), soci::use(id);
        tr.commit();

        // Wake the realtime worker to deliver the change immediately.
        app().rt().notifyChange();

        // Parse row to JSON
        const auto record = sociRow2Json(row, fields());

        // Extract all fields that have file/files as the underlying data
        std::vector<std::string> files_in_fields;
        std::ranges::for_each(fields(), [&](const json &field) {
            const auto &type = field["type"].get<std::string>();
            const auto &name = field["name"].get<std::string>();
            if (type == "file" && !record[name].is_null()) {
                const auto &file = record.value(name, "");
                if (!file.empty()) files_in_fields.emplace_back(file);
            }
            if (type == "files" && !record[name].is_null() && record[name].is_array()) {
                const auto &files = record.value(name, std::vector<std::string>{});
                // Expand the array data out
                for (const auto &file: files) {
                    if (!file.empty()) files_in_fields.emplace_back(file);
                }
            }
        });

        // For each file field, remove it in the filesystem
        Files::removeFiles(name(), files_in_fields);
    }

    // --------------------------------------------------------------------------- //
    // UTILS OPS                                                                   //
    // --------------------------------------------------------------------------- //

    int Entity::countRecords() const {
        // TODO add record filtering ...
        try {
            const auto sql = app().db().session();
            int count = 0;
            *sql << std::format("SELECT COUNT(id) FROM {}", sqlIdentifier(name())), soci::into(count);
            return count;
        } catch (std::exception &e) {
            throw MantisException(500, e.what());
        }
    }

    bool Entity::isEmpty() const {
        try {
            const auto &sql = app().db().session();
            int dummy = 0;
            soci::indicator ind = soci::i_null;
            *sql << std::format("SELECT 1 FROM {} LIMIT 1", sqlIdentifier(name())),
                    soci::into(dummy, ind);
            return ind == soci::i_null;
        } catch (const std::exception &e) {
            throw MantisException(500, e.what());
        }
    }

    std::optional<json> Entity::queryFromCols(const std::string &value, const std::vector<std::string> &columns) const {
        // Get a session object
        const auto sql = app().db().session();

        // Validate all column names against entity schema
        std::vector<std::string> valid_columns;
        for (const auto &col_name: columns) {
            if (hasField(col_name).has_value()) {
                valid_columns.push_back(col_name);
            } else {
                LogOrigin::entityWarn("Invalid Column",
                                      fmt::format("Invalid column name '{}' in queryFromCols for entity '{}'", col_name,
                                                  name()));
            }
        }

        if (valid_columns.empty()) {
            throw MantisException(400, "No valid columns provided");
        }

        // Build dynamic WHERE clause
        std::string where_clause;
        soci::values bind_values;
        for (size_t i = 0; i < valid_columns.size(); ++i) {
            const auto col = sqlIdentifier(valid_columns[i]);
            if (i > 0) where_clause += " OR ";
            where_clause += std::format("{0} = :{0}{1}", col, i);
            bind_values.set(std::format("{0}{1}", col, i), value);
        }

        const std::string query = std::format("SELECT * FROM {} WHERE {} LIMIT 1", sqlIdentifier(name()), where_clause);

        // Run query
        soci::row r;
        *sql << query, soci::use(bind_values), soci::into(r);

        if (sql->got_data())
            return sociRow2Json(r, fields());

        return std::nullopt;
    }

    bool Entity::recordExists(const std::string &id) const {
        try {
            std::string _nid;
            const auto sql = app().db().session();
            *sql << std::format("SELECT id FROM {} WHERE id = :id LIMIT 1", sqlIdentifier(name())),
                    soci::use(id), soci::into(_nid);
            return sql->got_data();
        } catch (soci::soci_error &e) {
            LogOrigin::entityTrace("Record Exists Error", e.what());
            return false;
        }
    }
}
