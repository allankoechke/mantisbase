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

#include <string>
#include <filesystem>
#include <chrono>
#include <argparse/argparse.hpp>
#include <dukglue/dukglue.h>

#include "core/types.h"
#include "core/kv_store.h"

namespace mb
{
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
     */
    class MantisBase
    {
    public:
        ~MantisBase();

        // Disable copying and moving
        MantisBase(const MantisBase&) = delete;
        MantisBase& operator=(const MantisBase&) = delete;
        MantisBase(MantisBase&&) = delete;
        MantisBase& operator=(MantisBase&&) = delete;

        /**
         * @brief Retrieve existing application instance.
         * @return A reference to the existing application instance.
         */
        static MantisBase& instance();

        /**
         * @brief Create class instance given cmd args passed in.
         * @see parseArgs() for expected cmd args to be passed in.
         *
         * @param argc Number of cmd args
         * @param argv Char array list
         * @return Reference to the created class instance
         */
        static MantisBase& create(int argc, char** argv);

        /**
         * @brief Convenience function to allow creating class instance given the
         * values needed to set up the app without any need for passing in cmd args.
         *
         * The expected values are:
         * {
         *     "database": "<db type>",
         *     "connection": "<connection string>",
         *     "dataDir": "<path to dir>",
         *     "publicDir": "<path to dir>",
         *     "scriptsDir": "<path to dir>",
         *     "dev": true,
         *     "serve": {
         *         "port": <int>,
         *         "host": "<host IP/addr>",
         *         "poolSize": <int>,
         *     },
         *     "admins": {
         *         "add": "<email to add>",
         *         "rm": "<email to remove>"
         *     }
         * }
         *
         * @note All primary options above are optional, you can omit to use default values.
         * @note `admins` subcommand expects a subcommand with either the `add` or `rm`.
         * @note `serve` command can have an empty json object and the app will configure with defaults.
         *
         * @code
         * json arg1 = json::object();
         * auto& app1 = MantisApp::create(arg2);
         *
         * json arg2;
         * arg2["database"] = "PSQL";
         * arg2["database"] = "dbname=mantis username=postgres password=12342532";
         * arg2["dev"] = true;
         * arg2["serve"] = json::object{};
         * auto& app2 = MantisApp::create(arg2);
         *
         * @endcode
         *
         * @param config JSON Object bearing the cmd args values to be used
         * @return A reference to the created class instance
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
        /// Get the logging unit object
        [[nodiscard]] Logger& log() const;
        /// Get the commandline parser object
        [[nodiscard]] argparse::ArgumentParser& cmd() const;
        /// Get the router object instance.
        [[nodiscard]] Router& router() const;
        /// Get the settings unit object
        [[nodiscard]] KVStore& settings() const;

        /**
         * @brief Fetch a table schema encapsulated by an `Entity` object from given the table name.
         * If table does not exist yet, return an emty object.
         *
         * @param entity_name Name of the table of interest
         * @return Entity object for the selected table
         */
        [[nodiscard]] Entity entity(const std::string& entity_name) const;

        /**
         * @brief Check if table schema encapsulated by an `Entity` object from given the table name exists.
         * If table does not exist yet, return false.
         *
         * @param entity_name Name of the table of interest
         * @return true if entity exists, false otherwise.
         */
        [[nodiscard]] bool hasEntity(const std::string& entity_name) const;

        /// Get the duktape context
        [[nodiscard]] duk_context* ctx() const;


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

#ifdef MANTIS_ENABLE_SCRIPTING
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
        std::string m_dbType;

        // System uptime checkpoint
        std::chrono::time_point<std::chrono::steady_clock> m_startTime;

        int m_port = 7070;
        std::string m_host = "127.0.0.1";

        int m_poolSize = 2;
        bool m_toStartServer = false;
        bool m_launchAdminPanel = false;
        bool m_isDevMode = false;

        std::unique_ptr<Logger> m_logger;
        std::unique_ptr<Database> m_database;
        std::unique_ptr<Router> m_router;
        std::unique_ptr<KVStore> m_kvStore;
        std::unique_ptr<argparse::ArgumentParser> m_opts;
        duk_context* m_dukCtx; // For duktape context
    };
}

#endif //MANTISBASE_APP_H
