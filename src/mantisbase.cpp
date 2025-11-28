#include "../include/mantisbase/mantis.h"
#include "../include/mantisbase/config.hpp"
#include "../include/mantisbase/core/models/entity.h"

#include <cmrc/cmrc.hpp>
#include <fstream>

namespace mantis
{
    MantisBase::MantisBase()
        : m_dbType("sqlite3"),
          m_startTime(std::chrono::steady_clock::now())
          , m_dukCtx(duk_create_heap_default())

    {
        // Enable Multi Sinks
        logger::init();
    }

    MantisBase::~MantisBase()
    {
        if (!m_toStartServer) {
            std::cout << std::endl;
            logger::info("Exitting, nothing else to do. Did you intend to run the server? Try `mantisapp serve` instead.");
        }

        // Terminate any shared pointers
        close();

        // Destroy duk context
        duk_destroy_heap(m_dukCtx);
    }

    void MantisBase::init(const int argc, char* argv[])
    {
        logger::info("Initializing Mantis, v{}", appVersion());

        // Set that the object was created successfully, now initializing
        m_isCreated = true;

        // Store cmd args into our member vector
        m_cmdArgs.reserve(argc);
        for (int i = 0; i < argc; ++i)
        {
            m_cmdArgs.emplace_back(argv[i]); // copy each string over
        }

        parseArgs(); // Parse args & start units

#ifdef MANTIS_ENABLE_SCRIPTING
        initJSEngine(); // Initialize JS engine
#endif
    }

    MantisBase& MantisBase::instance()
    {
        auto& app_instance = getInstanceImpl();
        if (!app_instance.m_isCreated)
            throw std::runtime_error("MantisBase not created yet");

        return app_instance;
    }

    MantisBase& MantisBase::create(const int argc, char** argv)
    {
        auto& app = getInstanceImpl();
        if (app.m_isCreated)
            throw std::runtime_error("MantisBase already created, use MantisBase::instance() instead.");

        // Initialize the app with passed in args
        app.init(argc, argv);

        // Return the instance
        return app;
    }

    MantisBase& MantisBase::create(const json& config)
    {
        auto& app = getInstanceImpl();
        if (app.m_isCreated)
            throw std::runtime_error("MantisBase already created, use MantisBase::instance() instead.");

        logger::trace("MantisBase Config: {}", config.dump());

        app.m_cmdArgs.emplace_back("mantisapp"); // Arg[0] should be the app name

        // dataDir publicDir scriptsDir serve [port host] admins [add remove]
        // --database psql
        if (config.contains("database"))
        {
            app.m_cmdArgs.emplace_back("--database");
            app.m_cmdArgs.push_back(config.at("database").get<std::string>());
        }

        // --connection "dbname=mantis host=127.0.0.1 username=duser password=1235"
        if (config.contains("connection"))
        {
            app.m_cmdArgs.emplace_back("--connection");
            app.m_cmdArgs.push_back(config.at("connection").get<std::string>());
        }

        // --dataDir /some/path/to/dir
        if (config.contains("dataDir"))
        {
            app.m_cmdArgs.emplace_back("--dataDir");
            app.m_cmdArgs.push_back(config.at("dataDir").get<std::string>());
        }

        // --publicDir /some/path/to/dir
        if (config.contains("publicDir"))
        {
            app.m_cmdArgs.emplace_back("--publicDir");
            app.m_cmdArgs.push_back(config.at("publicDir").get<std::string>());
        }

        // --scriptsDir /some/path/to/dir
        if (config.contains("scriptsDir"))
        {
            app.m_cmdArgs.emplace_back("--scriptsDir");
            app.m_cmdArgs.push_back(config.at("scriptsDir").get<std::string>());
        }

        // --dev
        if (config.contains("dev"))
        {
            app.m_cmdArgs.emplace_back("--dev"); // We don't care much about the value
        }

        // serve [--host x.y.z.t --port 1234 --poolSize 8]
        if (config.contains("serve"))
        {
            app.m_cmdArgs.emplace_back("serve");
            const auto& serve = config["serve"];

            // If we have a valid JSON Object
            if (serve.is_object())
            {

                // serve --host 127.0.0.1
                if (serve.contains("host"))
                {
                    app.m_cmdArgs.emplace_back("--host");
                    app.m_cmdArgs.push_back(serve.at("host").get<std::string>());
                }

                // serve --port 7071
                if (serve.contains("port"))
                {
                    app.m_cmdArgs.emplace_back("--port");
                    app.m_cmdArgs.push_back(std::to_string(serve.at("port").get<int>()));
                }

                // serve --poolSize 10
                if (serve.contains("poolSize"))
                {
                    app.m_cmdArgs.emplace_back("--poolSize");
                    app.m_cmdArgs.push_back(std::to_string(serve.at("poolSize").get<int>()));
                }
            }
        }

        // admins --add x@y.z
        // admins --rm x@y.z
        if (config.contains("admins"))
        {
            app.m_cmdArgs.emplace_back("admins");
            const auto& admins = config["admins"];

            if (admins.contains("add"))
            {
                app.m_cmdArgs.emplace_back("--host");
                app.m_cmdArgs.push_back(admins.at("host").get<std::string>());
            }

            if (admins.contains("rm"))
            {
                app.m_cmdArgs.emplace_back("--rm");
                app.m_cmdArgs.push_back(admins.at("rm").get<std::string>());
            }

            else
            {
                throw std::runtime_error("MantisBase `admins` command expects `--add <email>` or `--rm <user>` subcommand.");
            }
        }

        // Initialize the app with passed in args
        app.init();

        // Return the instance
        return app;
    }

    MantisBase& MantisBase::getInstanceImpl()
    {
        static MantisBase s_instance;
        return s_instance;
    }

    void MantisBase::init_units()
    {
        if (!ensureDirsAreCreated())
            quit(-1, "Failed to create database directories!");

        // Create instance objects
        m_logger = std::make_unique<LogsMgr>();
        m_exprEval = std::make_unique<ExprMgr>(); // depends on log()
        m_database = std::make_unique<Database>(); // depends on log()
        m_router = std::make_unique<Router>(); // depends on db() & http()
        m_kvStore = std::make_unique<KVStore>(); // depends on db(), router() & http()
        m_opts = std::make_unique<argparse::ArgumentParser>();
    }

    int MantisBase::quit(const int& exitCode, [[maybe_unused]] const std::string& reason)
    {
        // Stop server if running
        instance().close();

        if (exitCode != 0)
            logger::critical("Exiting Application with Code = {}", exitCode);

        std::exit(exitCode);
    }

    void MantisBase::close()
    {
        // Destroy instance objects
        if (m_opts) m_opts.reset();
        if (m_kvStore) m_kvStore.reset();
        if (m_router) m_router.reset();
        if (m_database) m_database.reset();
        if (m_exprEval) m_exprEval.reset();
        if (m_logger) m_logger.reset();
    }

    int MantisBase::run()
    {
        // Set server start time
        m_startTime = std::chrono::steady_clock::now();

#ifdef MANTIS_ENABLE_SCRIPTING
        // Load start script for Mantis
        loadStartScript();
#endif

        // If server command is explicitly passed in, start listening,
        // else, exit!
        if (m_toStartServer)
        {
            if (!m_router->listen())
                return 500;
        }

        return 0;
    }

    Database& MantisBase::db() const
    {
        return *m_database;
    }

    LogsMgr& MantisBase::log() const
    {
        return *m_logger;
    }

    argparse::ArgumentParser& MantisBase::cmd() const
    {
        return *m_opts;
    }

    Router& MantisBase::router() const
    {
        return *m_router;
    }

    ExprMgr& MantisBase::evaluator() const
    {
        return *m_exprEval;
    }

    KVStore& MantisBase::settings() const
    {
        return *m_kvStore;
    }

    Entity MantisBase::entity(const std::string &table_name) const {
        if (table_name.empty()) throw std::invalid_argument("Table name is invalid!");

        // Get schema cache from db, check if we have this data, return data if available
        const auto entity_obj = m_router->schemaCacheEntity(table_name);

        logger::trace("Fetched entity JSON: `{}`", entity_obj.schema().dump());

        return entity_obj;
    }

    duk_context* MantisBase::ctx() const
    {
        return m_dukCtx;
    }

    void MantisBase::openBrowserOnStart() const
    {
        // Return if flag is reset
        if (!m_launchAdminPanel) return;

        const std::string url = std::format("http://localhost:{}/admin", m_port);

#ifdef _WIN32
        std::string command = "start " + url;
#elif __APPLE__
        std::string command = "open " + url;
#elif __linux__
        std::string command = "xdg-open " + url;
#else
#error Unsupported platform
#endif

        if (int result = std::system(command.c_str()); result != 0)
        {
            logger::info("Could not open browser: {} > {}", command, result);
        }
    }

    std::chrono::time_point<std::chrono::steady_clock> MantisBase::startTime() const
    {
        return m_startTime;
    }

    bool MantisBase::isDevMode() const
    {
        return m_isDevMode;
    }

    void MantisBase::setDbType(const std::string& dbType)
    {
        if (dbType == "sqlite3" || dbType == "postgresql") {
            m_dbType = dbType;
            return;
        }

        throw std::invalid_argument("Expected database type of either `sqlite3` or `postgresql`");
    }

    std::string MantisBase::jwtSecretKey()
    {
        // This is the default secret key, override it through environment variable
        // MANTIS_JWT_SECRET, recommended to override this key
        // TODO add commandline input for overriding the key
        return getEnvOrDefault("MANTIS_JWT_SECRET", "<our-very-secret-JWT-key>");
    }

    std::string MantisBase::appVersion()
    {
        return getVersionString();
    }

    int MantisBase::appMinorVersion()
    {
        return MANTIS_VERSION_MINOR;
    }

    int MantisBase::appMajorVersion()
    {
        return MANTIS_VERSION_MAJOR;
    }

    int MantisBase::appPatchVersion()
    {
        return MANTIS_VERSION_PATCH;
    }

    std::string MantisBase::dbType() const
    {
        return m_dbType;
    }

    int MantisBase::port() const
    {
        return m_port;
    }

    void MantisBase::setPort(const int& port)
    {
        if (port < 0 || port > 65535)
            return;

        m_port = port;
        logger::debug("Setting Server Port to {}", port);
    }

    std::string MantisBase::host() const
    {
        return m_host;
    }

    void MantisBase::setHost(const std::string& host)
    {
        if (host.empty())
            return;

        m_host = host;
        logger::debug("Setting Server Host to {}", host);
    }

    int MantisBase::poolSize() const
    {
        return m_poolSize;
    }

    void MantisBase::setPoolSize(const int& pool_size)
    {
        if (pool_size <= 0)
            return;

        m_poolSize = pool_size;
    }

    std::string MantisBase::publicDir() const
    {
        return m_publicDir;
    }

    void MantisBase::setPublicDir(const std::string& dir)
    {
        if (dir.empty())
            return;

        m_publicDir = dir;
    }

    std::string MantisBase::dataDir() const
    {
        return m_dataDir;
    }

    void MantisBase::setDataDir(const std::string& dir)
    {
        if (dir.empty())
            return;

        m_dataDir = dir;
    }

    std::string MantisBase::scriptsDir() const
    {
        return m_scriptsDir;
    }

    void MantisBase::setScriptsDir(const std::string& dir)
    {
        if (dir.empty())
            return;

        m_scriptsDir = dir;
    }

    inline bool MantisBase::ensureDirsAreCreated() const
    {
        // Data Directory
        if (!createDirs(resolvePath(m_dataDir)))
            return false;

        if (!createDirs(resolvePath(m_publicDir)))
            return false;

        if (!createDirs(resolvePath(m_scriptsDir)))
            return false;

        return true;
    }

#ifdef MANTIS_ENABLE_SCRIPTING
    void MantisApp::initJSEngine()
    {
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

    void MantisApp::loadStartScript() const
    {
        // Look for index.js as the entry point
        const auto entryPoint = (fs::path(m_scriptsDir) / "index.mantis.js").string();
        loadAndExecuteScript(entryPoint);
    }

    void MantisApp::loadAndExecuteScript(const std::string& filePath) const
    {
        if (!fs::exists(fs::path(filePath)))
        {
            logger::trace("Executing a file that does not exist, path `{}`", filePath);
            return;
        }

        // If the file exists, lets load the contents and then execute
        std::ifstream file(filePath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string scriptContent = buffer.str();

        try
        {
            dukglue_peval<void>(m_dukCtx, scriptContent.c_str());
        }
        catch (const DukErrorException& e)
        {
            logger::critical("Error loading file at {} \n\tError: {}", filePath, e.what());
        }
    }

    void MantisApp::loadScript(const std::string& relativePath) const
    {
        // Construct full path relative to scripts directory
        const auto fullPath = fs::path(m_scriptsDir) / relativePath;
        loadAndExecuteScript(fullPath.string());
    }

    void MantisApp::quit_JSWrapper(const int code, const std::string& msg)
    {
        quit(code, msg);
    }

    DatabaseUnit* MantisApp::duk_db() const
    {
        return m_database.get();
    }

    RouterUnit* MantisApp::duk_router() const
    {
        return m_router.get();
    }
#endif
}
