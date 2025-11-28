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
#include "logger.h"

// #define __file__ "core/tables/database.h"

namespace mantis {
    using json = nlohmann::json;

    /**
     * @brief Database Management Class
     *
     * The class handles database connections, pooling and sessions.
     */
    class Database {
    public:
        Database();

        ~Database();

        /**
         * @brief Initializes the connection pool & connects to specific database.
         * @param conn_str Connection string for database containing username, password, etc. Not used for SQLite dbs.
         * @return A flag whether connection was successful or not.
         */
        bool connect(const std::string &conn_str);

        /**
         * @brief CLose all database connections and destroy connection pools.
         */
        void disconnect() const;

        /**
         * @brief Run database migrations, creates the default system tables.
         * @return `true` if migration completes successfully else `false`.
         */
        bool createSysTables() const;

        /**
         * @brief Get access to a session from the pool
         * @return A shared pointer to soci::session
         */
        [[nodiscard]] std::shared_ptr<soci::session> session() const;

        /**
         * Access to the underlying soci connection_pool instance
         * @return A reference to the soci::connection_pool instance
         */
        [[nodiscard]] soci::connection_pool &connectionPool() const;

        /**
         * @brief Check if the database is connected
         * @return Flag of the database connection
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
            logger::trace("$ sql << {}", query);
            // logger::trace("$ sql << {}\n\t└── Values ({})", query, params);
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
}

#endif //DATABASE_H
