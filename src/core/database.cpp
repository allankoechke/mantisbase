#include "../../include/mantisbase/core/database.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/logger.h"
#include "../../include/mantisbase/core/kv_store.h"
#include "../../include/mantisbase/utils/utils.h"

#include <private/soci-mktime.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include "mantisbase/core/models/entity_schema.h"

#if MANTIS_HAS_POSTGRESQL
#include <soci/postgresql/soci-postgresql.h>
#endif

// #define __file__ "core/tables/sys_tables.cpp"

namespace mb {
    Database::Database() : m_connPool(nullptr), mbApp(MantisBase::instance()) {
    }

    Database::~Database() {
        disconnect();
    }

    bool Database::connect(const std::string &conn_str) {
        // If pool size is invalid, just return
        if (mbApp.poolSize() <= 0)
            throw std::runtime_error("Session pool size must be greater than 0");

        // All databases apart from SQLite should pass in a connection string
        if (mbApp.dbType() != "sqlite3" && conn_str.empty())
            throw std::runtime_error("Connection string for database connection is required!");

        try {
            // Create connection pool instance
            m_connPool = std::make_unique<soci::connection_pool>(mbApp.poolSize());

            const auto db_type = mbApp.dbType();

            auto pool_size = static_cast<size_t>(mbApp.poolSize());
            // Populate the pools with db connections
            for (std::size_t i = 0; i < pool_size; ++i) {
                logger::trace("Creating db session for index `{}/{}`", i, pool_size);

                if (db_type == "sqlite3") {
                    // For SQLite, lets explicitly define location and name of the database
                    // we intend to use within the `dataDir`
                    auto sqlite_db_path = joinPaths(mbApp.dataDir(), "mantis.db").string();
                    soci::session &sql = m_connPool->at(i);

                    auto sqlite_conn_str = std::format(
                        "db={} timeout=30 shared_cache=true synchronous=normal foreign_keys=on", sqlite_db_path);
                    sql.open(soci::sqlite3, sqlite_conn_str);
                    sql.set_logger(new MantisLoggerImpl()); // Set custom query logger

                    // Log SQL insert values in DevMode only!
                    if (mbApp.isDevMode())
                        sql.set_query_context_logging_mode(soci::log_context::always);
                    else
                        sql.set_query_context_logging_mode(soci::log_context::on_error);

                    // Open SQLite in WAL mode, helps in enabling multiple readers, single writer
                    sql << "PRAGMA journal_mode=WAL";
                    sql << "PRAGMA wal_autocheckpoint=500"; // Checkpoint every 500 pages
                } else if (db_type == "postgresql") {
#if MANTIS_HAS_POSTGRESQL
                    // Connection Options
                    ///> Basic: "dbname=mydb user=scott password=tiger"
                    ///> With Host: "host=localhost port=5432 dbname=test user=postgres password=postgres");
                    ///> With Config: "dbname=mydatabase user=myuser password=mypass singlerows=true"

                    soci::session &sql = m_connPool->at(i);
                    sql.open(soci::postgresql, conn_str);
                    sql.set_logger(new MantisLoggerImpl()); // Set custom query logger

                    // Log SQL insert values in DevMode only!
                    if (MantisBase::instance().isDevMode())
                        sql.set_query_context_logging_mode(soci::log_context::always);
                    else
                        sql.set_query_context_logging_mode(soci::log_context::on_error);
#else
                    logger::warn("Database Connection for `PostgreSQL` has not been implemented yet!");
                    return false;
#endif
                } else if (db_type == "mysql") {
                    logger::warn("Database Connection for `MySQL` not implemented yet!");
                    return false;
                } else {
                    logger::warn("Database Connection to `{}` Not Implemented Yet!", conn_str);
                    return false;
                }
            }
        } catch (const std::exception &e) {
            logger::critical("Database Connection error: {}", e.what());
            return false;
        } catch (...) {
            logger::critical("Database Connection error: Unknown Error");
            return false;
        }

        if (MantisBase::instance().dbType() == "sqlite3") writeCheckpoint();

        return true;
    }

    void Database::disconnect() {
        // Make disconnect() safe to call multiple times and handle null/invalid states
        if (!m_connPool) {
            // Already disconnected or never connected, nothing to do
            return;
        }

        // Write checkpoint out (may fail if database is already closed, that's ok)
        writeCheckpoint();

        // Pool size cast - may fail if MantisBase instance is invalid, that's ok
        const auto pool_size = static_cast<size_t>(mbApp.poolSize());

        // Close all sessions in the pool
        for (std::size_t i = 0; i < pool_size; ++i) {
            try {
                if (soci::session &sess = m_connPool->at(i); sess.is_connected()) {
                    sess.close();
                    logger::debug("DB Shutdown: Closing soci::session object  {} of {} connections", i + 1, pool_size);
                } else {
                    logger::debug("DB Shutdown: soci::session object at index `{}` of {} connections is not connected.",
                                  i + 1, pool_size);
                }
            } catch (const soci::soci_error &e) {
                logger::critical("Database disconnection soci::error at index `{}`: {}", i, e.what());
            } catch (...) {
                // Ignore other errors during session close
            }
        }

        // Reset the connection pool
        m_connPool.reset();

        logger::debug("DB Shutdown: Session disconnection completed.");
    }

    bool Database::createSysTables() const {
        const auto sql = session();
        soci::transaction tr{*sql};

        try {
            // Create admin table, for managing and auth for admin accounts
            EntitySchema admin_schema{"mb_admins", "auth"};
            admin_schema.removeField("name");
            admin_schema.setSystem(true);
            *sql << admin_schema.toDDL();

            // Internal use for service accounts
            // Use `base` type to avoid login via /api/auth/*
            // Used for `id` to track given tokens for single use only
            EntitySchema service_schema{"mb_service_acc", "base"};
            service_schema.setSystem(true);
            service_schema.setHasApi(false);
            *sql << service_schema.toDDL();

            // Create and manage other db tables, keeping track of access rules, schema, etc.!
            EntitySchema tables_schema{"mb_tables", "base"};
            tables_schema.setSystem(true);
            tables_schema.addField(EntitySchemaField({
                {"name", "schema"}, {"type", "json"}, {"required", true}, {"system", true}
            }));
            *sql << tables_schema.toDDL();

            // A Key - Value settings store, where the key is hashed as the table id
            EntitySchema store_schema{"mb_store", "base"};
            store_schema.setSystem(true);
            store_schema.addField(EntitySchemaField({
                {"name", "value"}, {"type", "json"}, {"required", true}, {"system", true}
            }));
            *sql << store_schema.toDDL();

            // Commit changes
            tr.commit();

            // Enforce migration once settings object is created!
            // MantisBase::instance().settings().migrate();

            return true;
        } catch (std::exception &e) {
            tr.rollback();
            logger::critical("Create System Tables Failed: {}", e.what());
            return false;
        }
    }

    std::shared_ptr<soci::session> Database::session() const {
        return std::make_shared<soci::session>(*m_connPool);
    }

    soci::connection_pool &Database::connectionPool() const {
        return *m_connPool;
    }

    bool Database::isConnected() const {
        if (!m_connPool) return false;

        const auto sql = session();
        return sql->is_connected();
    }

    void Database::writeCheckpoint() const {
        // Make writeCheckpoint() safe to call during shutdown
        if (!m_connPool) {
            // No connection pool, nothing to checkpoint
            return;
        }

        // Enable this write checkpoint for SQLite databases ONLY
        try {
            if (mbApp.dbType() == "sqlite3") {
                try {
                    // Write out the WAL data to db file & truncate it
                    if (const auto sql = session(); sql->is_connected()) {
                        *sql << "PRAGMA wal_checkpoint(TRUNCATE)";
                    }
                } catch (std::exception &e) {
                    logger::critical("Database Connection SOCI::Error: {}", e.what());
                }
            }
        } catch (...) {
            // If MantisBase instance is invalid or dbType() fails, ignore it
            // This can happen during shutdown when instance is being torn down
        }
    }
#ifdef MANTIS_SCRIPTING_ENABLED
    void DatabaseUnit::registerDuktapeMethods() {
        const auto ctx = MantisApp::instance().ctx();

        // DatabaseUnit methods
        dukglue_register_property(ctx, &DatabaseUnit::isConnected, nullptr, "connected");
        dukglue_register_method(ctx, &DatabaseUnit::session, "session");
        dukglue_register_method_varargs(ctx, &DatabaseUnit::query, "query");

        // soci::session methods
        dukglue_register_method(ctx, &soci::session::close, "close");
        dukglue_register_method(ctx, &soci::session::reconnect, "reconnect");
        dukglue_register_property(ctx, &soci::session::is_connected, nullptr, "connected");
        dukglue_register_method(ctx, &soci::session::begin, "begin");
        dukglue_register_method(ctx, &soci::session::commit, "commit");
        dukglue_register_method(ctx, &soci::session::rollback, "rollback");
        dukglue_register_method(ctx, &soci::session::get_query, "getQuery");
        dukglue_register_method(ctx, &soci::session::get_last_query, "getLastQuery");
        dukglue_register_method(ctx, &soci::session::get_last_query_context, "getLastQueryContext");
        dukglue_register_method(ctx, &soci::session::got_data, "gotData");
        // dukglue_register_method(ctx, &soci::session::get_last_insert_id, "getLastInsertId");
        dukglue_register_method(ctx, &soci::session::get_backend_name, "getBackendName");
        dukglue_register_method(ctx, &soci::session::empty_blob, "emptyBlob");
    }
#endif


#ifdef MANTIS_SCRIPTING_ENABLED
    duk_ret_t DatabaseUnit::query(duk_context *ctx) {
        // TRACE_CLASS_METHOD();

        // Get number of arguments
        const int nargs = duk_get_top(ctx);

        if (nargs < 1) {
            logger::critical("[JS] Expected at least 1 argument (query string)");
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "Expected at least 1 argument (query string)");
            return DUK_RET_TYPE_ERROR;
        }

        // First argument is the SQL query
        const char *query = duk_require_string(ctx, 0);
        // logger::trace("[JS] SQL Query: `{}`", query);

        // Collect remaining arguments (bind parameters)
        soci::values vals;

        try {
            for (int i = 1; i < nargs; i++) {
                if (!duk_is_object(ctx, i)) {
                    logger::critical("[JS] Arguments after query must be objects.");
                    duk_error(ctx, DUK_ERR_TYPE_ERROR, "Arguments after query must be objects");
                    return DUK_RET_TYPE_ERROR;
                }

                // Convert JavaScript object to JSON string
                duk_dup(ctx, i); // Duplicate the object at index i
                const char *json_char = duk_json_encode(ctx, -1);

                // Create a string out of the char*, else, json::parse throws an exception
                const std::string json_str = json_char ? std::string(json_char) : std::string();

                duk_pop(ctx); // Pop the encoded string

                nlohmann::json json_obj = json::object();
                try {
                    // Parse into nlohmann::json
                    json_obj = nlohmann::json::parse(json_str);
                } catch (const std::exception &e) {
                    logger::critical("[JS] Parsing exception: {}", e.what());
                } catch (const char *e) {
                    logger::critical("[JS] Unknown Parsing exception");
                }
                logger::trace("After Parsing, object? `{}`", json_obj.dump());

                for (auto &[key, value]: json_obj.items()) {
                    if (value.is_string()) {
                        auto str_val = value.get<std::string>();
                        logger::trace("[JS] Str Value: `{}` - `{}`", key, str_val);
                        vals.set(key, str_val);
                        logger::trace("[JS] After Set Value");
                        logger::trace("[JS] After Set Value To: `{}`", vals.get<std::string>(key));
                    } else if (value.is_number_integer()) {
                        int int_val = value.get<int>();
                        logger::trace("[JS] Int Value: `{}`", int_val);
                        vals.set(key, int_val);
                    } else if (value.is_number_float()) {
                        double double_val = value.get<double>();
                        logger::trace("[JS] Double Value: `{}`", double_val);
                        vals.set(key, double_val);
                    } else if (value.is_boolean()) {
                        bool bool_val = value.get<bool>();
                        logger::trace("[JS] Bool Value: `{}`", bool_val);
                        vals.set(key, bool_val);
                    } else if (value.is_null()) {
                        std::optional<int> val;
                        logger::trace("[JS] Null Value: `null`");
                        vals.set(key, val, soci::i_null);
                    } else if (value.is_object() || value.is_array()) {
                        logger::trace("[JS] JSON Value: `{}`", value.dump());
                        vals.set(key, value);
                    } else {
                        auto err = std::format("Could not cast type at {} to DB supported types.", (i - 1));
                        logger::critical("[JS] Casting Value > {}", err);
                        duk_error(ctx, DUK_ERR_TYPE_ERROR, err.c_str());
                        return DUK_RET_TYPE_ERROR;
                    }
                }
            }
        } catch (const std::exception &e) {
            logger::critical("[JS] Getting Binding Values Failed: Why? {}", e.what());
        }

        // Get SQL Session
        auto sql = session();

        logger::trace("[JS] soci::value binding? {}", nargs - 1);

        // Execute SQL Statement
        soci::row data_row;
        soci::statement st = nargs < 2 // If only the query is provided, no binding values
                                 ? (sql->prepare << query, soci::into(data_row)) // Execute only the query
                                 : (sql->prepare << query, soci::use(vals), soci::into(data_row));
        st.execute(); // Execute statement

        json results = json::array();
        while (st.fetch()) {
            nlohmann::json obj = rowToJson(data_row);
            results.push_back(obj);
        }

        logger::trace("[JS] Results: {}", results.dump());

        if (results.empty()) {
            // Return null
            duk_push_null(ctx);
            return 1;
        }

        // Convert nlohmann::json to JavaScript object
        std::string results_str = results.size() == 1 ? results[0].dump() : results.dump();
        duk_push_string(ctx, results_str.c_str());
        duk_json_decode(ctx, -1);
        return 1; // Return the object
    }
#endif
} // namespace mb
