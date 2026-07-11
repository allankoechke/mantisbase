#include "../include/mantisbase/mantis.h"
#include "../include/mantisbase/config.hpp"
#include "../include/mantisbase/core/models/entity.h"
#include "../include/mantisbase/core/kv_store.h"
#include "../include/mantisbase/core/realtime.h"

#include <cmrc/cmrc.hpp>
#include <fstream>
#include <memory>

namespace mb {
    MantisBase::MantisBase()
        : m_dbType("sqlite3"),
          m_startTime(std::chrono::steady_clock::now()),
          m_logger(std::make_unique<Logger>()) {}
          // , m_dukCtx(duk_create_heap_default()) {}

    MantisBase::~MantisBase() {
        // if (!m_toStartServer) {
        //     std::cout << std::endl;
        //     LogOrigin::info(
        //         "Exiting Application",
        //         "Nothing else to do. Did you intend to run the server? Try `mantisbase serve` instead.");
        // }

        // Destroy duk context
        // duk_destroy_heap(m_dukCtx);

        // std::cout << "Exiting ~MantisBase()" << std::endl;
    }

    void MantisBase::init(const int argc, char *argv[]) {
        // Set that the object was created successfully, now initializing
        m_isCreated.store(true);

        // Store cmd args into our member vector (skip when pre-built by create(json))
        if (argc > 0) {
            m_cmdArgs.clear();
            m_cmdArgs.reserve(argc);
            for (int i = 0; i < argc; ++i) {
                m_cmdArgs.emplace_back(argv[i]); // copy each string over
            }
        }

        parseArgs(); // Parse args & start units

#ifdef MB_SCRIPTING_ENABLED
        initJSEngine(); // Initialize JS engine
#endif
    }

    MantisBase &MantisBase::instance() {
        auto &app_instance = getInstanceImpl();
        if (!app_instance.isCreated())
            throw std::runtime_error("MantisBase not created yet");

        return app_instance;
    }

    MantisBase &MantisBase::create(const int argc, char **argv) {
        auto &app = getInstanceImpl();
        if (app.isCreated())
            throw std::runtime_error("MantisBase already created, use MantisBase::instance() instead.");

        // Initialize the app with passed in args
        app.init(argc, argv);

        // Return the instance
        return app;
    }

    MantisBase &MantisBase::create(const json &config) {
        auto &app = getInstanceImpl();
        if (app.isCreated())
            throw std::runtime_error("MantisBase already created, use MantisBase::instance() instead.");

        // logEntry::trace("MantisBase Config: {}", config.dump());

        app.m_cmdArgs.emplace_back("mantisbase");

        if (config.contains("database") || config.contains("db")) {
            app.m_cmdArgs.emplace_back("--db");
            app.m_cmdArgs.push_back(config.contains("db")
                                         ? config.at("db").get<std::string>()
                                         : config.at("database").get<std::string>());
        }

        if (config.contains("connection") || config.contains("db_url")) {
            app.m_cmdArgs.emplace_back("--db_url");
            app.m_cmdArgs.push_back(config.contains("db_url")
                                        ? config.at("db_url").get<std::string>()
                                        : config.at("connection").get<std::string>());
        }

        if (config.contains("dataDir") || config.contains("data-dir")) {
            app.m_cmdArgs.emplace_back("--data-dir");
            app.m_cmdArgs.push_back(config.contains("data-dir")
                                        ? config.at("data-dir").get<std::string>()
                                        : config.at("dataDir").get<std::string>());
        }

        if (config.contains("publicDir") || config.contains("public-dir")) {
            app.m_cmdArgs.emplace_back("--public-dir");
            app.m_cmdArgs.push_back(config.contains("public-dir")
                                        ? config.at("public-dir").get<std::string>()
                                        : config.at("publicDir").get<std::string>());
        }

        if (config.contains("scriptsDir") || config.contains("scripts-dir")) {
            app.m_cmdArgs.emplace_back("--scripts-dir");
            app.m_cmdArgs.push_back(config.contains("scripts-dir")
                                        ? config.at("scripts-dir").get<std::string>()
                                        : config.at("scriptsDir").get<std::string>());
        }

        if (config.contains("migrationsDir") || config.contains("migrations-dir")) {
            app.m_cmdArgs.emplace_back("--migrations-dir");
            app.m_cmdArgs.push_back(config.contains("migrations-dir")
                                        ? config.at("migrations-dir").get<std::string>()
                                        : config.at("migrationsDir").get<std::string>());
        }

        if (config.contains("dev")) {
            app.m_cmdArgs.emplace_back("--dev");
        }

        if (config.contains("serve")) {
            app.m_cmdArgs.emplace_back("serve");
            const auto &serve = config["serve"];

            if (serve.is_object()) {
                if (serve.contains("host")) {
                    app.m_cmdArgs.emplace_back("--host");
                    app.m_cmdArgs.push_back(serve.at("host").get<std::string>());
                }

                if (serve.contains("port")) {
                    app.m_cmdArgs.emplace_back("--port");
                    app.m_cmdArgs.push_back(std::to_string(serve.at("port").get<int>()));
                }

                if (serve.contains("skip-admin-setup") && serve.at("skip-admin-setup").get<bool>()) {
                    app.m_cmdArgs.emplace_back("--skip-admin-setup");
                }

                if (serve.contains("pool-size") || serve.contains("poolSize")) {
                    app.m_cmdArgs.emplace_back("--pool-size");
                    app.m_cmdArgs.push_back(std::to_string(serve.contains("pool-size")
                                                                 ? serve.at("pool-size").get<int>()
                                                                 : serve.at("poolSize").get<int>()));
                }
            }
        }

        if (config.contains("admins")) {
            app.m_cmdArgs.emplace_back("admins");
            const auto &admins = config["admins"];

            if (admins.contains("add")) {
                app.m_cmdArgs.emplace_back("--add");
                if (admins["add"].is_array()) {
                    app.m_cmdArgs.push_back(admins["add"].at(0).get<std::string>());
                    app.m_cmdArgs.push_back(admins["add"].at(1).get<std::string>());
                } else if (admins["add"].is_string()) {
                    app.m_cmdArgs.push_back(admins["add"].get<std::string>());
                    if (admins.contains("password"))
                        app.m_cmdArgs.push_back(admins.at("password").get<std::string>());
                }
            } else if (admins.contains("ls") && admins["ls"].get<bool>()) {
                app.m_cmdArgs.emplace_back("--ls");
            } else if (admins.contains("rm")) {
                app.m_cmdArgs.emplace_back("--rm");
                app.m_cmdArgs.push_back(admins.at("rm").get<std::string>());
            } else {
                throw std::runtime_error(
                    "MantisBase `admins` command expects `add`, `ls`, or `rm`.");
            }
        }

        if (config.contains("migrations")) {
            app.m_cmdArgs.emplace_back("migrations");
            const auto &migrations = config["migrations"];
            if (migrations.contains("up") && migrations["up"].get<bool>())
                app.m_cmdArgs.emplace_back("--up");
            else if (migrations.contains("down") && migrations["down"].get<bool>())
                app.m_cmdArgs.emplace_back("--down");
        }

        if (config.contains("schema")) {
            app.m_cmdArgs.emplace_back("schema");
            const auto &schema = config["schema"];
            if (schema.contains("ls") && schema["ls"].get<bool>()) {
                app.m_cmdArgs.emplace_back("--ls");
            } else if (schema.contains("rm")) {
                app.m_cmdArgs.emplace_back("--rm");
                app.m_cmdArgs.push_back(schema.at("rm").get<std::string>());
            } else if (schema.contains("add")) {
                app.m_cmdArgs.emplace_back("--add");
                app.m_cmdArgs.push_back(schema.at("add").get<std::string>());
            } else if (schema.contains("update")) {
                app.m_cmdArgs.emplace_back("--update");
                app.m_cmdArgs.push_back(schema.at("update").at("entity").get<std::string>());
                app.m_cmdArgs.push_back(schema.at("update").at("body").get<std::string>());
            }
        }

        // Initialize the app with passed in args
        app.init();

        // Return the instance
        return app;
    }

    MantisBase &MantisBase::getInstanceImpl() {
        static std::unique_ptr<MantisBase> s_instance(new MantisBase());
        return *s_instance;
    }

    void MantisBase::init_units() {
        if (!ensureDirsAreCreated())
            quit(-1, "Failed to create database directories!");

        // Create instance objects
        m_database = std::make_unique<Database>(); // depends on log()
        m_realtime = std::make_unique<RealtimeDB>(); // depends on db()
        m_router = std::make_unique<Router>(); // depends on db() & http()
        m_kvStore = std::make_unique<KeyValStore>(); // depends on db(), router() & http()
        m_opts = std::make_unique<argparse::ArgumentParser>();
    }

    int MantisBase::quit(const int &exitCode, [[maybe_unused]] const std::string &reason) {
        // Stop server if running
        instance().close();

        if (exitCode != 0)
            LogOrigin::critical("Application Exit",
                fmt::format("Exiting Application with Code = {}", exitCode));

        std::exit(exitCode);
    }

    void MantisBase::close() {
        // Already closed, nothing to do
        if (!isCreated()) {
            return;
        }

        LogOrigin::trace("MantisBase", "Closing units");
        try {
            if (m_router && m_router->server().is_running()) {
                m_router->close();
                LogOrigin::trace("Router", "Router stopped");
            }

            if (m_kvStore) {
                // m_kvStore.close();
                // LogOrigin::trace("KV Store Shutdown", "[MB] Finished KV Store closing ...");
            }

            if (m_database && m_database->isConnected()) {
                m_database->disconnect();
                LogOrigin::trace("Db Shutdown", "Completed Successfully!");
            }
        } catch (const std::exception &e) {
            std::cerr << "Error during reset: " << e.what() << std::endl;
        }

        // Reset created flag
        m_isCreated.store(false);

        // Mark logger instance as destroyed
        Logger::isDbInitialized.store(false);
        LogOrigin::debug("MantisBase", "Shutdown Complete");
    }

    int MantisBase::run() {
        // Set server start time
        m_startTime = std::chrono::steady_clock::now();

#ifdef MB_SCRIPTING_ENABLED
        // Load start script for Mantis
        loadStartScript();
#endif

        // If server command is explicitly passed in, start listening,
        // else, exit!
        if (m_toStartServer) {
            if (!m_router->listen())
                return 500;
        }

        return 0;
    }

    Database &MantisBase::db() const {
        return *m_database;
    }

    argparse::ArgumentParser &MantisBase::cmd() const {
        return *m_opts;
    }

    Router &MantisBase::router() const {
        return *m_router;
    }

    KeyValStore &MantisBase::settings() const {
        return *m_kvStore;
    }

    Logger &MantisBase::logs() const {
        return *m_logger;
    }
    RealtimeDB & MantisBase::rt() const {
        return *m_realtime;
    }

    Entity MantisBase::entity(const std::string &entity_name) const {
        if (!EntitySchema::isValidEntityName(entity_name))
            throw MantisException(400,
                                  std::format("Invalid entity name `{}` provided.", entity_name));

        // Get schema cache from db, check if we have this data, return data if available
        const auto entity_obj = m_router->schemaCacheEntity(entity_name);
        return entity_obj;
    }

    bool MantisBase::hasEntity(const std::string &entity_name) const {
        return m_router->hasSchemaCache(entity_name);
    }

    // duk_context *MantisBase::ctx() const {
    //     return m_dukCtx;
    // }

    void MantisBase::openBrowserOnStart() const {
        // Skip spinning the default browser on first boot
        // Useful for tests
        if (getEnvOrDefault("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "0") == "1") return;

        std::cout << std::endl;

        try {
            // Create service account
            const auto s_acc = entity("mb_service_acc");
            auto record = s_acc.create({});

            // Claims
            nlohmann::json claims;
            claims["id"] = record["id"];
            claims["entity"] = "mb_service_acc";

            // Create token and pass it in
            const auto token = Auth::createToken(claims, 30 * 60); // Token valid for 30mins

            const std::string url = std::format("http://localhost:{}/mb/setup?token={}", m_port, token);
            LogOrigin::info("Admin Setup", fmt::format(
                                "Open link below to setup first admin user. Note, token valid for 30mins only.\n\t— {}\n\t— Alternatively use mantisbase admins --add <email> <password>\n",
                                url));

#ifdef _WIN32
            const std::string command = "start " + url;
#elif __APPLE__
            const std::string command = "open " + url;
#elif __linux__
            const std::string command = "xdg-open " + url;
#else
            LogOrigin::critical("Unsupported Platform", "Unsupported platform");
#endif

            if (int result = std::system(command.c_str()); result != 0) {
                LogOrigin::info("Browser Open Failed", fmt::format("Could not open browser, result code: {}", result));
            }
        } catch (const std::exception &e) {
            LogOrigin::critical("Admin Dashboard Failed",
                                fmt::format("Failed to spin admin dashboard\n\t— {}", e.what()));
        }

        std::cout << std::endl;
    }

    std::chrono::time_point<std::chrono::steady_clock> MantisBase::startTime() const {
        return m_startTime;
    }

    bool MantisBase::isDevMode() const {
        return m_isDevMode;
    }

    void MantisBase::setDbType(const std::string &dbType) {
        if (dbType == "sqlite3" || dbType == "postgresql") {
            m_dbType = dbType;
            return;
        }

        throw MantisException(500, "Expected database type of either `sqlite3` or `postgresql` but got {}", dbType);
    }

    std::string MantisBase::jwtSecretKey() {
        // This is the default secret key, override it through environment variable
        // MB_JWT_SECRET, recommended to override this key
        // TODO add commandline input for overriding the key
        // or explictly require that key to be set before we boot.
        return getEnvOrDefault("MB_JWT_SECRET", "<our-very-secret-JWT-key>");
    }

    std::string MantisBase::appVersion() {
        return getVersionString();
    }

    int MantisBase::appMinorVersion() {
        return MB_VERSION_MINOR;
    }

    int MantisBase::appMajorVersion() {
        return MB_VERSION_MAJOR;
    }

    int MantisBase::appPatchVersion() {
        return MB_VERSION_PATCH;
    }

    std::string MantisBase::dbType() const {
        return m_dbType;
    }

    int MantisBase::port() const {
        return m_port;
    }

    void MantisBase::setPort(const int &port) {
        if (port < 0 || port > 65535)
            return;

        m_port = port;
        LogOrigin::debug("Server Configuration", fmt::format("Setting Server Port to {}", port));
    }

    std::string MantisBase::host() const {
        return m_host;
    }

    void MantisBase::setHost(const std::string &host) {
        if (host.empty())
            return;

        m_host = host;
        LogOrigin::debug("Server Configuration", fmt::format("Setting Server Host to {}", host));
    }

    int MantisBase::poolSize() const {
        return m_poolSize;
    }

    void MantisBase::setPoolSize(const int &pool_size) {
        if (pool_size <= 0)
            return;

        m_poolSize = pool_size;
    }

    std::string MantisBase::publicDir() const {
        return m_publicDir;
    }

    void MantisBase::setPublicDir(const std::string &dir) {
        if (dir.empty())
            return;

        m_publicDir = dir;
    }

    std::string MantisBase::dataDir() const {
        return m_dataDir;
    }

    void MantisBase::setDataDir(const std::string &dir) {
        if (dir.empty())
            return;

        m_dataDir = dir;
    }

    std::string MantisBase::scriptsDir() const {
        return m_scriptsDir;
    }

    void MantisBase::setScriptsDir(const std::string &dir) {
        if (dir.empty())
            return;

        m_scriptsDir = dir;
    }

    std::string MantisBase::migrationsDir() const {
        return m_migrationsDir;
    }

    void MantisBase::setMigrationsDir(const std::string &dir) {
        if (dir.empty())
            return;

        m_migrationsDir = dir;
    }

    bool MantisBase::skipAdminSetup() const {
        if (m_skipAdminSetup)
            return true;

        return getEnvOrDefault("MB_SKIP_ADMIN_SETUP", "0") == "1";
    }

    void MantisBase::setSkipAdminSetup(const bool skip) {
        m_skipAdminSetup = skip;
    }

    inline bool MantisBase::ensureDirsAreCreated() const {
        // Data Directory
        if (!createDirs(resolvePath(m_dataDir)))
            return false;

        if (!createDirs(resolvePath(m_publicDir)))
            return false;

        if (!createDirs(resolvePath(m_scriptsDir)))
            return false;

        if (!createDirs(resolvePath(m_migrationsDir)))
            return false;

        return true;
    }

#ifdef MB_SCRIPTING_ENABLED
    void MantisApp::initJSEngine() {
        // TRACE_CLASS_METHOD();

        // ---------------------------------------------- //
        // Register `app` object
        // ---------------------------------------------- //
        // Register the singleton instance as a global
        dukglue_register_global(m_dukCtx, this, "app");

        // Properties
        dukglue_register_property(m_dukCtx, &MantisApp::host, &MantisApp::setHost, "host");
        dukglue_register_property(m_dukCtx, &MantisApp::port, &MantisApp::setPort, "port");
        dukglue_register_property(m_dukCtx, &MantisApp::poolSize, &MantisApp::setPoolSize, "poolSize");
        dukglue_register_property(m_dukCtx, &MantisApp::publicDir, &MantisApp::setPublicDir, "publicDir");
        dukglue_register_property(m_dukCtx, &MantisApp::dataDir, &MantisApp::setDataDir, "dataDir");
        dukglue_register_property(m_dukCtx, &MantisApp::isDevMode, nullptr, "devMode");
        dukglue_register_property(m_dukCtx, &MantisApp::dbTypeByName, nullptr, "dbType");
        dukglue_register_property(m_dukCtx, &MantisApp::jwtSecretKey_JSWrapper, nullptr, "secretKey");
        dukglue_register_property(m_dukCtx, &MantisApp::version_JSWrapper, nullptr, "version");

        // `app.close()`
        dukglue_register_method(m_dukCtx, &MantisApp::close, "close");
        // `app.quit(1, "Just crashed?")`
        dukglue_register_method(m_dukCtx, &MantisApp::quit_JSWrapper, "quit");
        // `app.db()`
        dukglue_register_method(m_dukCtx, &MantisApp::duk_db, "db");
        // `app.router()`
        dukglue_register_method(m_dukCtx, &MantisApp::duk_router, "router");

        MantisRequest::registerDuktapeMethods();
        MantisResponse::registerDuktapeMethods();

        // ---------------------------------------------- //
        // Register `console` object
        // ---------------------------------------------- //
        // Create console object and register methods
        duk_push_object(m_dukCtx);

        duk_push_c_function(m_dukCtx, &DuktapeImpl::nativeConsoleInfo, DUK_VARARGS);
        duk_put_prop_string(m_dukCtx, -2, "info");

        duk_push_c_function(m_dukCtx, &DuktapeImpl::nativeConsoleTrace, DUK_VARARGS);
        duk_put_prop_string(m_dukCtx, -2, "trace");

        duk_push_c_function(m_dukCtx, &DuktapeImpl::nativeConsoleInfo, DUK_VARARGS);
        duk_put_prop_string(m_dukCtx, -2, "log");

        duk_put_global_string(m_dukCtx, "console");

        // UTILS methods
        registerUtilsToDuktapeEngine();

        // DATABASE methods
        DatabaseUnit::registerDuktapeMethods();

        // Router methods for dukttape
        RouterUnit::registerDuktapeMethods();
    }

    void MantisApp::loadStartScript() const {
        // Look for index.js as the entry point
        const auto entryPoint = (fs::path(m_scriptsDir) / "index.mantis.js").string();
        loadAndExecuteScript(entryPoint);
    }

    void MantisApp::loadAndExecuteScript(const std::string &filePath) const {
        if (!fs::exists(fs::path(filePath))) {
            LogOrigin::trace("File Execution",
                             fmt::format("Executing a file that does not exist, path `{}`", filePath));
            return;
        }

        // If the file exists, lets load the contents and then execute
        std::ifstream file(filePath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string scriptContent = buffer.str();

        try {
            dukglue_peval<void>(m_dukCtx, scriptContent.c_str());
        } catch (const DukErrorException &e) {
            LogOrigin::critical("File Load Error",
                                fmt::format("Error loading file at {} \n\tError: {}", filePath, e.what()));
        }
    }

    void MantisApp::loadScript(const std::string &relativePath) const {
        // Construct full path relative to scripts directory
        const auto fullPath = fs::path(m_scriptsDir) / relativePath;
        loadAndExecuteScript(fullPath.string());
    }

    void MantisApp::quit_JSWrapper(const int code, const std::string &msg) {
        quit(code, msg);
    }

    DatabaseUnit *MantisApp::duk_db() const {
        return m_database.get();
    }

    RouterUnit *MantisApp::duk_router() const {
        return m_router.get();
    }
#endif
}
