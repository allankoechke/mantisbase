/**
 * @file database_mgr.h
 * @brief Database Unit file for managing base database functionality: connecton, pooling, logging, etc.
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <memory>
#include <soci/soci.h>
#include <nlohmann/json.hpp>
#include <mantisbase/core/private-impl/soci_custom_types.hpp>

#ifdef MANTIS_SCRIPTING_ENABLED
#include <dukglue/dukglue.h>
#endif

#include "../mantisbase.h"
#include "../utils/utils.h"
#include "logger/logger.h"

namespace mb {
    using json = nlohmann::json;

    /**
     * @brief Database connection and session management.
     *
     * Handles database connections, connection pooling, and provides
     * session management for executing queries. Supports SQLite (default)
     * and PostgreSQL.
     *
     * @code
     * Database db;
     * db.connect("dbname=mantis user=postgres password=pass");
     * auto session = db.session();
     * *session << "SELECT * FROM users", soci::into(rows);
     * @endcode
     */
    class Database {
    public:
        /**
         * @brief Construct database instance.
         */
        Database();

        /**
         * @brief Destructor (disconnects from database).
         */
        ~Database();

        /**
         * @brief Connect to database and initialize connection pool.
         * @param conn_str Connection string (format depends on database type)
         *   - SQLite: path to database file or empty for default
         *   - PostgreSQL: "dbname=name host=host port=5432 user=user password=pass"
         * @return true if connection successful, false otherwise
         */
        bool connect(const std::string &conn_str);

        /**
         * @brief Close all database connections and destroy connection pool.
         */
        void disconnect();

        /**
         * @brief Create system tables (mb_tables, mb_admins, etc.).
         * @return true if migration successful, false otherwise
         */
        bool createSysTables() const;

        /**
         * @brief Get a database session from the connection pool.
         * @return Shared pointer to soci::session
         * @code
         * auto sql = db.session();
         * *sql << "SELECT * FROM users WHERE id = :id", soci::use(id), soci::into(row);
         * @endcode
         */
        [[nodiscard]] std::shared_ptr<soci::session> session() const;

        /**
         * @brief Get access to the underlying connection pool.
         * @return Reference to soci::connection_pool
         */
        [[nodiscard]] soci::connection_pool &connectionPool() const;

        /**
         * @brief Check if database is connected.
         * @return true if connected, false otherwise
         */
        [[nodiscard]] bool isConnected() const;

#ifdef MANTIS_SCRIPTING_ENABLED
        static void registerDuktapeMethods();
#endif

    private:
        /**
         * @brief Execute an SQL script, bound to any given values and return a single or no
         * Dukvalue object. In case of any error, we throw the error back to JS.
         *
         * @code
         * const user = query("SELECT * FROM students WHERE id = :id LIMIT 1", {id: "12345643"})
         * if(user!==undefined) {
         *     // ....
         * }
         * query("INSERT INTO students(name, age) VALUES ()", {name: "John Doe"}, {age: 12})
         * @endcode
         *
         * @param ctx duktape context object
         * @return Execution status of the query. The objects returned (JSON Object or JSON Array) are
         * pushed directly to the JS context
         */
#ifdef MANTIS_SCRIPTING_ENABLED
        duk_ret_t query(duk_context *ctx);
#endif

        /**
         * @brief Write WAL data to db file and truncate the WAL file
         */
        void writeCheckpoint() const;

        std::unique_ptr<soci::connection_pool> m_connPool;
        const MantisBase &mbApp;
    };

    /**
     * @brief Logger implementation for soci, allowing us to override the default logging behaviour
     * with our own custom logger.
     */
    class MantisLoggerImpl : public soci::logger_impl {
    public:
        /**
         * @brief Called before query is executed by soci, we can log the query here.
         * @param query SQL Query to be executed
         */
        void start_query(std::string const &query) override {
            logger_impl::start_query(query);
            LogOrigin::dbTrace(fmt::format("$ sql << {}", query));
        }

    private:
        /**
         * @brief Obtain a pointer to the logger implementation
         * @return Logger implementation pointer
         */
        logger_impl *do_clone() const override {
            return new MantisLoggerImpl();
        }
    };
} // mb

#endif //DATABASE_H
