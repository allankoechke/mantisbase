/**
 * @file mantisbase.h
 *
 * @brief The main application for mantisbase.
 *
 * Controls all other units creation, commandline parsing as well as handling application state.
 *
 * Created by allan on 16/05/2025.
 */

#ifndef MANTISBASE_APP_H
#define MANTISBASE_APP_H

#include <memory>
#include <string>
#include <filesystem>
#include <chrono>
#include "export.h"
#include <argparse/argparse.hpp>
#ifdef MB_SCRIPTING_ENABLED
#include <dukglue/dukglue.h>
#endif

#include "core/types.h"
#include "core/kv_store.h"

namespace mb
{
    class RealtimeDB;
    /**
     * @brief MantisBase entry point.
     *
     * This class handles the entrypoint to the `Mantis` world, where we can
     * set/get application flags and variables, as well as access other
     * application units. These units are:
     * - DatabaseMgr: For all database handling, @see DatabaseMgr for more information.
     * - HttpMgr: Low-level http server operation and routing. @see HttpMgr for low-level access or @see RouterMgr for a high level routing methods.
     * - LogsMgr: For logging capabilities, @see LoggingMgr for more details.
     * - RouterMgr: High level routing wrapper on top of @see HttpMgr, @see RouterMgr for more details.
     * - ValidatorMgr: A validation store using regex, @see Validator for more details.
     *
     * @note MantisBase is a single-instance-per-process type (a managed
     * singleton): the whole codebase reaches the active instance through
     * instance(). Call create() exactly once during startup, then use
     * instance() everywhere else to obtain that same instance. Multiple
     * concurrent instances in one process are not supported — create() throws
     * if an instance already exists, and instance() throws if one has not been
     * created yet. "Embeddable" means you link and drive MantisBase from your
     * own C++ process, not that you can run several independent instances side
     * by side.
     */
    class MANTISBASE_API MantisBase
    {
    public:
        ~MantisBase();

        // Disable copying and moving
        MantisBase(const MantisBase&) = delete;
        MantisBase& operator=(const MantisBase&) = delete;
        MantisBase(MantisBase&&) = delete;
        MantisBase& operator=(MantisBase&&) = delete;

        /**
         * @brief Retrieve the existing application instance.
         * @return A reference to the single, already-created application instance.
         * @throws std::runtime_error if create() has not been called yet.
         */
        static MantisBase& instance();

        /**
         * @brief Create class instance given cmd args passed in.
         * @see parseArgs() for expected cmd args to be passed in.
         *
         * @note Only one instance may exist per process. Call this once at
         *       startup and use instance() thereafter.
         *
         * @param argc Number of cmd args
         * @param argv Char array list
         * @return Reference to the created class instance
         * @throws std::runtime_error if an instance has already been created.
         */
        static MantisBase& create(int argc, char** argv);

        /**
         * @brief Convenience function to allow creating class instance given the
         * values needed to set up the app without any need for passing in cmd args.
         *
         * The expected values are:
         * {
         *     "db": "<db type>",
         *     "db_url": "<connection string>",
         *     "data-dir": "<path to dir>",
         *     "public-dir": "<path to dir>",
         *     "scripts-dir": "<path to dir>",
         *     "migrations-dir": "<path to dir>",
         *     "dev": true,
         *     "serve": {
         *         "port": <int>,
         *         "host": "<host IP/addr>",
         *         "pool-size": <int>,
         *         "skip-admin-setup": <bool>
         *     },
         *     "admins": {
         *         "add": ["<email>", "<password>"],
         *         "ls": true,
         *         "rm": "<email or id>"
         *     },
         *     "migrations": { "up": true, "down": false },
         *     "schema": { "ls": true, "rm": "<entity>", "add": "<json>", "update": { "entity": "<name>", "body": "<json>" } }
         * }
         *
         * @note All primary options above are optional, you can omit to use default values.
         * @note `admins` subcommand expects a subcommand with either the `add` or `rm`.
         * @note `serve` command can have an empty json object and the app will configure with defaults.
         *
         * @note Only one instance may exist per process; call this once during
         *       startup and use instance() thereafter.
         *
         * @code
         * // Defaults only:
         * auto& app = MantisBase::create(json::object());
         *
         * // With configuration:
         * json cfg;
         * cfg["db"] = "postgresql";
         * cfg["db_url"] = "dbname=mantis user=postgres password=12342532";
         * cfg["dev"] = true;
         * cfg["serve"] = json::object();
         * auto& app = MantisBase::create(cfg);
         * // ...elsewhere in the process, retrieve the same instance:
         * auto& same = MantisBase::instance();
         * @endcode
         *
         * @param config JSON Object bearing the cmd args values to be used
         * @return A reference to the created class instance
         * @throws std::runtime_error if an instance has already been created.
         */
        static MantisBase& create(const json& config = json::object());

        /**
         * @brief Start the http server and start listening for requests.
         * @return `0` if execution was okay, else a non-zero value.
         */
        [[nodiscard]] int run();

        /**
         * @brief Close the application and reset object
         * instances that are dependent on the class
         *
         * Internally, this stops running http server,
         * disconnects from the database and does any required
         * cleanup
         */
        void close();

        /**
         * @brief Quit the running application immediately.
         * @param exitCode Exit code value
         * @param reason User-friendly reason for the exit.
         * @return `exitCode` value.
         */
        static int quit(const int& exitCode = 0, const std::string& reason = "Something went wrong!");

        /**
         * @brief Retrieve HTTP Listening port.
         * @return Http Listening Port.
         */
        [[nodiscard]] int port() const;
        /**
         * @brief Set a new port value for HTTP server
         * @param port New HTTP port value.
         */
        void setPort(const int& port);

        /**
         * @brief Retrieve the database pool size value.
         * @return SOCI's database pool size.
         */
        [[nodiscard]] int poolSize() const;

        /**
         * @brief Retrieve HTTP Server host address. For instance, a host of `127.0.0.1`, `0.0.0.0`, etc.
         * @return HTTP Server Host address.
         */
        [[nodiscard]] std::string host() const;
        /**
         * @brief Update HTTP Server host address.
         * @param host New HTTP Server host address.
         */
        void setHost(const std::string& host);

        /**
         * @brief Retrieve the public static file directory.
         * @return MantisApp public directory.
         */
        [[nodiscard]] std::string publicDir() const;
        /**
         * @brief Update HTTP server static file directory.
         * @param dir New directory path.
         */
        void setPublicDir(const std::string& dir);

        /**
         * @brief Retrieves the data directory where SQLite db and files are stored.
         * @return MantisApp data directory.
         */
        [[nodiscard]] std::string dataDir() const;
        /**
         * @brief Update the data directory for MantisApp.
         * @param dir New data directory.
         */
        void setDataDir(const std::string& dir);

        /**
         * @brief Retrieves the scripts directory where JavaScript files are stored
         * used for extending functionality in Mantis.
         *
         * @return MantisApp scripts directory.
         */
        [[nodiscard]] std::string scriptsDir() const;
        /**
         * @brief Update the scripts directory for MantisApp.
         * @param dir New scripts directory.
         */
        void setScriptsDir(const std::string& dir);

        /**
         * @brief Retrieves the migrations directory path.
         * @return Migrations directory.
         */
        [[nodiscard]] std::string migrationsDir() const;
        /**
         * @brief Update the migrations directory path.
         * @param dir New migrations directory.
         */
        void setMigrationsDir(const std::string& dir);

        /**
         * @brief Whether first-boot admin setup (browser) should be skipped on serve.
         */
        [[nodiscard]] bool skipAdminSetup() const;
        void setSkipAdminSetup(bool skip);

        /**
         * @brief Retrieves the active database type.
         * @return Selected DatabaseType enum value.
         */
        [[nodiscard]] std::string dbType() const;

        /**
         * Update the active database type for Mantis.
         * @param dbType New database type enum value.
         */
        void setDbType(const std::string& dbType);
        /**
         * @brief Retrieve the JWT secret key.
         * @return JWT Secret value.
         */
        static std::string jwtSecretKey();
        /**
         * Fetch the application version
         * @return Application version
         */
        static std::string appVersion();
        /// Fetch the major version
        static int appMinorVersion();
        /// Fetch the minor version
        static int appMajorVersion();
        /// Fetch the patch version
        static int appPatchVersion();

        /// Get the database unit object
        [[nodiscard]] Database& db() const;
        /// Get the commandline parser object
        [[nodiscard]] argparse::ArgumentParser& cmd() const;
        /// Get the router object instance.
        [[nodiscard]] Router& router() const;
        /// Get the KeyValue unit object
        [[nodiscard]] KeyValStore& settings() const;
        /// Get the logs unit object
        [[nodiscard]] Logger& logs() const;
        /// Get the realtime unit (SQLite/PostgreSQL change detection for SSE /api/v1/realtime).
        [[nodiscard]] RealtimeDB& rt() const;

        /**
         * @brief Fetch a table schema encapsulated by an `Entity` object from given the table name.
         * If table does not exist yet, return an emty object.
         *
         * @param entity_name Name of the table of interest
         * @return Entity object for the selected table
         */
        [[nodiscard]] Entity&& entity(const std::string& entity_name) const;

        /**
         * @brief Check if table schema encapsulated by an `Entity` object from given the table name exists.
         * If table does not exist yet, return false.
         *
         * @param entity_name Name of the table of interest
         * @return true if entity exists, false otherwise.
         */
        [[nodiscard]] bool hasEntity(const std::string& entity_name) const;

        /// Get the duktape context
#ifdef MB_SCRIPTING_ENABLED
        [[nodiscard]] duk_context* ctx() const;
#endif


        /**
         * @brief Launch browser with the admin dashboard page. If all goes well, the default
         * OS browser should open (if not opened) with the admin dashboard URL.
         *
         * > Added in v0.1.6
         */
        void openBrowserOnStart() const;

        /**
         * @brief Get the server start time in std::chrono::
         *
         * @return Server start time
         */
        [[nodiscard]] std::chrono::time_point<std::chrono::steady_clock> startTime() const;

        [[nodiscard]] bool isDevMode() const;

    private:
        // Make class creation private to enforce
        // singleton app pattern.
        MantisBase();

        /**
         * @brief Run initialization actions for Mantis, ensuring all objects are initialized properly before use.
         */
        void init(int argc = 0, char* argv[] = {});

        /**
         * Creates static instance if not created yet and returns it.
         *
         * @return instance of the MantisApp class
         */
        static MantisBase& getInstanceImpl();

        /**
         * @brief Set the database pool size value.
         * @param pool_size New pool size value.
         */
        void setPoolSize(const int& pool_size);


        // Private members
        void parseArgs(); ///> Parse command-line arguments
        void init_units(); ///> Initialize application units

        [[nodiscard]]
        bool ensureDirsAreCreated() const; /// Ensures we created all required directories

        [[nodiscard]] bool isCreated() const;

#ifdef MB_SCRIPTING_ENABLED
        /**
         * @brief Initialize JS engine and register Mantis functions to JS
         */
        void initJSEngine();

        /**
         * @brief Load startup `.js` file `index.mantis.js` from the mantis
         * scripts directory.
         */
        void loadStartScript() const;

        /**
         * @brief Load and execute a passed in file path for a `.js` file.
         *
         * @param filePath File path to load and execute
         */
        void loadAndExecuteScript(const std::string& filePath) const;

        /**
         * @brief Load a script file and execute it
         *
         * @param relativePath Relative file path to be loaded.
         */
        void loadScript(const std::string& relativePath) const;

        std::string version_JSWrapper() const { return appVersion(); }
        std::string jwtSecretKey_JSWrapper() const { return jwtSecretKey(); }
        void quit_JSWrapper(int code, const std::string& msg);

        /**
         * @brief Wrapper method to return `DatabaseUnit*` instead of
         * `DatabaseUnit&` returned by @see db() method.
         *
         * @return DatabaseUnit instance pointer
         */
        [[nodiscard]] Database* duk_db() const;
        [[nodiscard]] Router* duk_router() const;
#endif

        // Store commandline args passed in, to be used in the init phase.
        std::vector<std::string> m_cmdArgs;

        // Hold state if the instance has be initialized already!
        std::atomic<bool> m_isCreated = false;

        std::string m_publicDir;
        std::string m_dataDir;
        std::string m_scriptsDir;
        std::string m_migrationsDir;
        std::string m_dbType;

        // System uptime checkpoint
        std::chrono::time_point<std::chrono::steady_clock> m_startTime;

        int m_port = 7070;
        std::string m_host = "127.0.0.1";

        // For snowflake ID generation
        uint8_t m_zoneId = 0;
        uint8_t m_shardNo = 0;

        //
        int m_poolSize = 2;
        bool m_toStartServer = false;
        bool m_launchAdminPanel = false;
        bool m_isDevMode = false;
        bool m_skipAdminSetup = false;

        std::unique_ptr<Logger> m_logger;
        std::unique_ptr<Database> m_database;
        std::unique_ptr<RealtimeDB> m_realtime;
        std::unique_ptr<Router> m_router;
        std::unique_ptr<KeyValStore> m_kvStore;
        std::unique_ptr<argparse::ArgumentParser> m_opts;
#ifdef MB_SCRIPTING_ENABLED
        duk_context* m_dukCtx = nullptr;
#endif
    };
}

#endif //MANTISBASE_APP_H
