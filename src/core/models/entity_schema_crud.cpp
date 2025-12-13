//
// Created by codeart on 12/11/2025.
//

#include "../../../include/mantisbase/core/models/entity_schema.h"
#include "../../../include/mantisbase/utils/utils.h"

#include <soci/soci.h>

#include "mantisbase/core/exceptions.h"

namespace mb {
    nlohmann::json EntitySchema::listTables(const json &) {
        const auto sql = MantisBase::instance().db().session();
        const soci::rowset rs = (sql->prepare <<
                                 "SELECT id, schema, created, updated FROM mb_tables");

        auto response = json::array();
        for (const auto &row: rs) {
            const auto id = row.get<std::string>(0);
            const auto schema = row.get<json>(1);
            const auto created = dbDateToString(row, 2);
            const auto updated = dbDateToString(row, 3);

            response.push_back({
                {"id", id},
                {"schema", schema},
                {"created", created},
                {"updated", updated}
            });
        }

        return response;
    }

    nlohmann::json EntitySchema::getTable(const std::string &table_id) {
        if (table_id.empty())
            throw MantisException(400, "Required table id/name is empty!");

        const auto sql = MantisBase::instance().db().session();

        soci::row row;
        *sql << "SELECT id, schema, created, updated FROM mb_tables WHERE id = :id", soci::use(table_id),
                soci::into(row);

        if (!sql->got_data())
            throw MantisException(404, "No table for given id/name `" + table_id + "`");

        const auto schema = row.get<json>(1);
        const auto created = dbDateToString(row, 2);
        const auto updated = dbDateToString(row, 3);

        return {
            {"id", table_id},
            {"schema", schema},
            {"created", created},
            {"updated", updated}
        };
    }

    nlohmann::json EntitySchema::createTable(const EntitySchema &new_table) {
        // Database session & transaction instance
        const auto sql = MantisBase::instance().db().session();
        soci::transaction tr(*sql);

        try {
            if (const auto err = new_table.validate(); err.has_value())
                throw MantisException(400, err.value());

            const auto id = new_table.id();
            const auto schema = new_table.toJSON();

            // Check if item exits already in db
            if (tableExists(schema.at("name").get<std::string>())) {
                throw MantisException(500, "Table with similar name exists.");
            }

            // Create default time values
            const std::time_t t = time(nullptr);
            std::tm created_tm = *std::localtime(&t);

            // Execute DDL & Save to DB
            logger::trace("Creating table with data with schema: \n{}", schema.dump(4));

            // Insert to mb_tables
            *sql << "INSERT INTO mb_tables (id, schema, created, updated) VALUES (:id, :schema, :created, :updated)",
                    soci::use(id), soci::use(schema), soci::use(created_tm), soci::use(created_tm);

            // Create actual SQL table
            *sql << new_table.toDDL();
            tr.commit();

            json obj;
            obj["id"] = id;
            obj["schema"] = schema;
            obj["created"] = tmToStr(created_tm);
            obj["updated"] = tmToStr(created_tm);

            // Create files directory
            Files::createDir(new_table.name());

            // Add created table to the routes
            MantisBase::instance().router().addSchemaCache(schema);

            return obj;
        } catch (...) {
            tr.rollback();
            throw;
        }
    }

    nlohmann::json EntitySchema::updateTable(const std::string &table_id, const nlohmann::json &new_schema) {
        if (new_schema.empty())
            throw MantisException(400, "Schema body is empty!");

        const auto sql = MantisBase::instance().db().session();
        soci::transaction tr(*sql);

        try {
            // Get saved database record
            nlohmann::json old_schema; // Original Entity Schema
            std::tm created_tm; // Created timestamp
            *sql << "SELECT schema, created FROM mb_tables WHERE id = :id",
                    soci::use(table_id), soci::into(old_schema), soci::into(created_tm);

            if (!sql->got_data()) {
                throw MantisException(404, "Entity resource for given name/id was not found!");
            }

            EntitySchema old_entity = EntitySchema::fromSchema(old_schema); // Old table data
            EntitySchema new_entity{old_entity}; // New table data
            assert(old_entity == new_entity); // These two objects should be same
            new_entity.updateWith(new_schema);

            // Validate new object
            if (auto err = new_entity.validate(); err.has_value())
                throw MantisException(400, err.value());

            // --------- Handle Field(s) Changes ---------------- //
            if (new_schema.contains("fields")) {
                for (const auto &field: new_schema["fields"]) {
                    if (field.contains("op") && field["op"].is_string() && !field["op"].empty()) {
                        auto id = field.contains("id") ? field["id"].get<std::string>() : "";
                        logger::trace("Field id: {}", id);

                        if (id.empty()) {
                            throw MantisException(400, "Expected an `id` in field for `op` operations.");
                        }

                        if (!old_entity.hasFieldById(id)) {
                            throw MantisException(400, "No field found with id `" + id + "` for `op` operations.");
                        }

                        const auto op = field["op"].get<std::string>();
                        if (op == "delete" || op == "remove") {
                            auto &f = old_entity.fieldById(id);

                            // If the field is valid, generate drop colum statement and execute!
                            *sql << sql->get_backend()->drop_column(old_entity.name(), trim(f.name()));
                            continue;
                        }

                        throw MantisException(400, "Field `op` expected `remove` or `delete` value only.");
                    }
                }
            }

            // Get db type
            const auto db_type = MantisBase::instance().dbType();

            const auto new_schema_fields = new_schema.contains("fields") && new_schema["fields"].is_array()
                                               ? new_schema["fields"]
                                               : json::array();

            for (const auto &entity_field_schema: new_schema_fields) {
                // Check if the field exists in the database already, then:
                //  > If it exists: Update its particulars
                //  > Else, create the field

                if (entity_field_schema.contains("id")
                    && old_entity.hasFieldById(entity_field_schema["id"].get<std::string>())) {
                    // Extract `id` from the schema JSON
                    auto e_id = entity_field_schema["id"].get<std::string>();

                    // Extract old field by `id`
                    auto &old_field = old_entity.fieldById(e_id);

                    // Create and update a copy
                    auto new_field = EntitySchemaField(old_field).updateWith(entity_field_schema);

                    // Check if field name was changed
                    if (old_field.name() != new_field.name()) {
                        auto entity_name = old_entity.name();
                        auto old_name = old_field.name();
                        auto new_name = new_field.name();
                        *sql << "ALTER TABLE :table_name RENAME COLUMN :old_field_name TO :new_field_name",
                                soci::use(entity_name), soci::use(old_name), soci::use(new_name);
                    }

                    // Check if `required` flag was updated
                    if (old_field.required() != new_field.required()) {
                        // ------------- SQLite -----------------
                        // SQLite requires recreating the table afresh to get the changes
                        if (db_type == "sqlite3")
                            throw MantisException(
                                500, "Adding/dropping required constraints not supported on SQLite databases!");

                        // Get the state
                        bool is_required = new_field.required();

                        // ------------- POSTGRESQL -----------------
                        if (db_type == "postgresql") {
                            const auto &entity_name = old_entity.name();
                            const auto &field_name = new_field.name();

                            if (is_required) {
                                *sql << "ALTER TABLE :table ALTER COLUMN :column SET NOT NULL;",
                                        soci::use(entity_name), soci::use(field_name);
                            } else {
                                *sql << "ALTER TABLE :table ALTER COLUMN :column DROP NOT NULL;",
                                        soci::use(entity_name), soci::use(field_name);
                            }
                        }

                        // ------------- MYSQL -----------------
                        else if (db_type == "mysql") {
                            auto table = old_entity.name();
                            auto column = new_field.name();
                            auto d_type = sql->get_backend()->create_column_type(new_field.toSociType(), 0, 0);

                            if (new_field.required()) {
                                *sql << "ALTER TABLE :table MODIFY :column :datatype NOT NULL;",
                                        soci::use(table), soci::use(column), soci::use(d_type);
                            } else {
                                *sql << "ALTER TABLE :table MODIFY :column :datatype NULL;",
                                        soci::use(table), soci::use(column), soci::use(column);
                            }
                        } else {
                            throw MantisException(500, "Database type not supported.");
                        }
                    }

                    if (old_field.isPrimaryKey() != new_field.isPrimaryKey()) {
                        // ------------- SQLite ----------------- //
                        if (db_type == "sqlite3")
                            throw MantisException(
                                500, "Adding/dropping primary key constraints not supported on SQLite databases!");

                        // ------------- POSTGRESQL ----------------- //
                        if (db_type == "postgresql") {
                            auto table = old_entity.name();
                            auto column = new_field.name();

                            if (new_field.required()) {
                                *sql << "ALTER TABLE :table ADD CONSTRAINT pk_:column PRIMARY KEY (:column);",
                                        soci::use(table), soci::use(column), soci::use(column);
                            } else {
                                *sql << "ALTER TABLE :table DROP CONSTRAINT pk_:column;",
                                        soci::use(table), soci::use(column);
                            }
                        }

                        // ------------- MYSQL ----------------- //
                        else {
                            auto table = old_entity.name();
                            auto column = new_field.name();
                            if (new_field.required()) {
                                *sql << "ALTER TABLE :table ADD PRIMARY KEY (:column);",
                                        soci::use(table), soci::use(column);
                            } else {
                                *sql << "ALTER TABLE :table DROP PRIMARY KEY;",
                                        soci::use(table);
                            }
                        }
                    }

                    if (old_field.isUnique() != new_field.isUnique()) {
                        // ------------- SQLite ----------------- //
                        if (db_type == "sqlite3")
                            throw MantisException(
                                500, "Adding/dropping unique constraints not supported on SQLite databases!");

                        // ------------- POSTGRESQL ----------------- //
                        if (db_type == "postgresql") {
                            if (new_field.isUnique()) {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                *sql << "ALTER TABLE :table DROP CONSTRAINT uniq_:column;",
                                        soci::use(table), soci::use(column);
                            } else {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                *sql << "ALTER TABLE :table ADD CONSTRAINT uniq_:column UNIQUE (:column);",
                                        soci::use(table), soci::use(column), soci::use(column);
                            }
                        }

                        // ------------- MYSQL ----------------- //
                        else {
                            if (new_field.isUnique()) {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                *sql << "ALTER TABLE :table DROP INDEX uniq_:column;",
                                        soci::use(table), soci::use(column);
                            } else {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                *sql << "ALTER TABLE :table ADD CONSTRAINT uniq_:column UNIQUE (:column);",
                                        soci::use(table), soci::use(column), soci::use(column);
                            }
                        }
                    }

                    if (old_field.constraint("default_value") != new_field.constraint("default_value")) {
                        // ------------- SQLite ----------------- //
                        if (db_type == "sqlite3")
                            throw MantisException(
                                500, "Changing default value for a column not supported in SQLite database.");

                        // ------------- POSTGRESQL ----------------- //
                        if (db_type == "postgresql") {
                            // If value is empty, reset default value
                            if (new_field.constraint("default_value").is_null()) {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                *sql << "ALTER TABLE :table ALTER COLUMN :column DROP DEFAULT;",
                                        soci::use(table), soci::use(column);
                            } else {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                auto def_val =
                                        toDefaultSqlValue(new_field.type(), new_field.constraint("default_value"));
                                *sql << "ALTER TABLE :table ALTER COLUMN :column SET DEFAULT :default;",
                                        soci::use(table), soci::use(column), soci::use(def_val);
                            }
                        }

                        // ------------- MYSQL ----------------- //
                        if (db_type == "mysql") {
                            // If value is empty, reset default value
                            if (new_field.constraint("default_value").is_null()) {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                *sql << "ALTER TABLE :table ALTER :column DROP DEFAULT;",
                                        soci::use(table), soci::use(column);
                            } else {
                                auto table = old_entity.name();
                                auto column = new_field.name();
                                auto def_val =
                                        toDefaultSqlValue(new_field.type(), new_field.constraint("default_value"));
                                *sql << "ALTER TABLE :table ALTER :column SET DEFAULT :default;",
                                        soci::use(table), soci::use(column), soci::use(def_val);
                            }
                        }
                    }
                }
                // Field does not exist in the entity yet, create one.
                else {
                    EntitySchemaField new_field(entity_field_schema);

                    // Create item
                    std::string query = sql->get_backend()->add_column(old_entity.name(), new_field.name(),
                                                                       new_field.toSociType(), 0, 0);

                    if (new_field.isPrimaryKey()) query += " PRIMARY KEY";
                    if (new_field.required()) query += " NOT NULL";
                    if (new_field.isUnique()) query += " UNIQUE";
                    if (auto def_val = new_field.constraint("default_value"); !def_val.is_null()) {
                        query += " DEFAULT ";
                        query += toDefaultSqlValue(new_field.type(), def_val);
                    }
                    *sql << query;
                }
            }

            // --------- Handle View Query Changes -------------- //
            // std::string m_viewSqlQuery;
            if (old_entity.viewQuery() != new_entity.viewQuery()) {
                throw MantisException(500, "View query has not been implemented yet!");

                // TODO
                // - Update view query
                // - Drop all queries and recreate them to match new fields
            }

            // --------- Handle Type Changes -------------------- //
            if (old_entity.type() != new_entity.type()) {
                throw MantisException(500, "Changing entity type is currently not supported.");
            }

            // --------- Handle Name Changes -------------------- //
            if (old_entity.name() != new_entity.name()) {
                *sql << "ALTER TABLE " + old_entity.name() + " RENAME TO " + new_entity.name();
            }

            // Get updated timestamp
            std::time_t t = time(nullptr);
            std::tm updated_tm = *std::localtime(&t);

            // Update table record, if all went well.
            std::string query = "UPDATE mb_tables SET id = :id, schema = :schema,";
            query += " updated = :updated WHERE id = :old_id";

            std::string old_id = old_entity.id();
            std::string new_id = new_entity.id();
            json new_schema = new_entity.toJSON();
            *sql << query, soci::use(new_id), soci::use(new_schema),
                    soci::use(updated_tm), soci::use(old_id);

            // Write out any pending changes ...
            tr.commit();


            json record;
            record["id"] = new_entity.id();
            record["schema"] = new_schema;
            record["created"] = tmToStr(created_tm);
            record["updated"] = tmToStr(updated_tm);

            // Wrap in new try catch block to suppress any errors here to avoid rollbacks
            try {
                // Update cache & subsequently the routes ...
                MantisBase::instance().router().updateSchemaCache(old_entity.name(), new_schema);

                // Only trigger routes to be reloaded if table name changes.
                if (old_entity.name() != new_entity.name()) {
                    // Update file table folder name
                    Files::renameDir(old_entity.name(), new_entity.name());
                }
            } catch (std::exception &e) {
                logger::critical("Error updating entity schema\n\t- {}", e.what());
            }

            return record;
        } catch (const MantisException &e) {
            tr.rollback();
            logger::critical("Error Updating EntitySchema\n\t- {}", e.what());
            throw;
        } catch (const std::exception &e) {
            tr.rollback();
            logger::critical("Error Updating EntitySchema\n\t- {}", e.what());
            throw MantisException(500, e.what());
        }
    }

    void EntitySchema::dropTable(const EntitySchema &original_table) {
        return dropTable(original_table.id());
    }

    void EntitySchema::dropTable(const std::string &table_id) {
        TRACE_METHOD();
        const auto sql = MantisBase::instance().db().session();
        soci::transaction tr(*sql);

        try {
            // Check if item exists of given id
            nlohmann::json schema;
            *sql << "SELECT schema FROM mb_tables WHERE id = :id",
                    soci::use(table_id), soci::into(schema);

            if (!sql->got_data()) {
                throw MantisException(404, "EntitySchema with given id `" + table_id + "` was not found!");
            }

            const auto entity_name = schema.at("name").get<std::string>();

            // Remove from DB
            *sql << "DELETE FROM mb_tables WHERE id = :id", soci::use(table_id);
            *sql << "DROP TABLE IF EXISTS " + entity_name;

            // Delete files directory
            Files::deleteDir(entity_name);

            // Remove route for this Entity
            MantisBase::instance().router().removeSchemaCache(entity_name);

            tr.commit();
        } catch (const MantisException &e) {
            tr.rollback();
            logger::critical("Error dropping table schema: {}", e.what());
            throw;
        } catch (const std::exception &e) {
            tr.rollback();
            logger::critical("Error dropping table schema: {}", e.what());
            throw MantisException(500, e.what());
        }
    }

    bool EntitySchema::tableExists(const std::string &table_name) {
        try {
            const auto db_type = MantisBase::instance().dbType();
            const auto sql = MantisBase::instance().db().session();

            if (!(db_type == "sqlite3" || db_type == "postgresql" || db_type == "mysql")) {
                throw std::runtime_error("The database `" + db_type + "` is not supported yet!");
            }
            const std::string query = db_type == "sqlite3"
                                          ? "SELECT name FROM sqlite_master WHERE type = 'table' AND name = :name;"
                                          : db_type == "postgresql"
                                                ? "SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = 'public' AND table_name = :name);"
                                                : "SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = :name);";

            soci::row db_data;
            *sql << query, soci::use(table_name, "name"), soci::into(db_data);
            return sql->got_data();
        } catch (const std::exception &e) {
            logger::critical("Error checking table in database: {}", e.what());
            throw MantisException(500, e.what());
        }
    }

    bool EntitySchema::tableExists(const EntitySchema &table) {
        return tableExists(table.name());
    }
} // mantis
