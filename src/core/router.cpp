#include "../../include/mantisbase/core/router.h"
#include "../../include/mantisbase/utils/utils.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/database.h"
#include "../../include/mantisbase/core/files.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/kv_store.h"

#include <cmrc/cmrc.hpp>
#include <chrono>
#include <thread>
#include <drogon/drogon.h>

// For thread logger
#include <memory>
#include <shared_mutex>
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <spdlog/sinks/ansicolor_sink.h>

#include "../include/mantisbase/core/exceptions.h"
#include "../include/mantisbase/core/middlewares.h"
#include "../include/mantisbase/core/models/entity_schema.h"
#include "../include/mantisbase/core/models/entity_routes.h"
#include "../include/mantisbase/core/models/entity_schema_routes.h"
#include "../include/mantisbase/core/logger/log_database.h"
#include "../include/mantisbase/core/realtime.h"
#include "../include/mantisbase/core/sse.h"
#include "../include/mantisbase/core/ws.h"
#include "../include/mantisbase/core/oauth.h"
#include "../include/mantisbase/core/api_keys.h"
#include "../include/mantisbase/utils/snowflake.hpp"

// Declare a mantis namespace for the embedded FS
CMRC_DECLARE(mantis);

namespace mb {
    Router::Router(const MantisBase &app)
        : mApp(app),
          m_sseMgr(std::make_unique<SSEMgr>(app)) {
        // Add global middlewares to work across all routes
        m_preRoutingMiddlewares.push_back(getAuthToken());
        m_preRoutingMiddlewares.push_back(hydrateContextData());

        m_sfId.init(1, 1);
    }

    Router::~Router() {
        if (m_running.load())
            close();
    }

    bool Router::init() { {
            // Runs before the server starts listening (single-threaded), but we
            // take the exclusive lock anyway to keep all m_entityMap access
            // uniformly synchronized and future-proof.
            std::unique_lock lock(m_entityMapMutex);

            const auto sql = mApp.db().session();
            const soci::rowset rows = (sql->prepare << "SELECT schema FROM mb_tables");

            for (const auto &row: rows) {
                const auto schema = row.get<nlohmann::json>("schema");

                // Create entity based on the schema, bound to this application
                // so its CRUD ops can reach db/realtime without the singleton.
                Entity entity{mApp, schema};

                // Store this object to keep alive function pointers
                // if not, possible access violation error
                m_entityMap.emplace(entity.name(), std::move(entity));
            }

            // Add admin routes
            EntitySchema admin_schema{mApp, "mb_admins", "auth"};
            admin_schema.removeField("name");
            admin_schema.setSystem(true);
            auto admin_entity = admin_schema.toEntity();
            m_entityMap.emplace(admin_entity.name(), std::move(admin_entity));

            // Service Schema [No routes]
            EntitySchema service_schema{mApp, "mb_service_acc", "base"};
            service_schema.setHasApi(false);
            service_schema.setSystem(true);
            auto service_entity = service_schema.toEntity();
            m_entityMap.emplace(service_entity.name(), std::move(service_entity));
        }

        // Misc Endpoints [admin, auth, etc]
        generateMiscEndpoints();

        return true;
    }

    bool Router::listen() {
        try {
            const auto host = mApp.host();
            const auto port = mApp.port();

            // Get admin entity, query all admins
            const auto admin_entity = mApp.entity("mb_admins");

            // If we don't have admin accounts, spin up admin dashboard
            bool launch_admin_setup = !mApp.skipAdminSetup() && admin_entity.isEmpty();

            m_sseMgr->start();

            // Configure Drogon
            drogon::app()
                    .addListener(host, port)
                    .setThreadNum(4);

            drogon::app().registerSyncAdvice([this](const drogon::HttpRequestPtr &req) -> drogon::HttpResponsePtr {
                // Generate and store request ID in attributes
                std::string requestId = fmt::format("req_{}", m_sfId.nextID());
                req->attributes()->insert("request_id", requestId);

                // Return nullptr to continue normal processing
                return nullptr;
            });

            // Register logger func for all requests
            drogon::app().registerPostHandlingAdvice(
                [&](const drogon::HttpRequestPtr &req,
                    const drogon::HttpResponsePtr &resp) {
                    const auto start = req->creationDate();
                    const auto end = trantor::Date::now();
                    const auto duration = end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch();
                    auto seconds = static_cast<double>(duration) / 1000000.0;

                    LogOrigin::info("HTTP",
                                    fmt::format("{} {} {}{} {}s {}B {} {} {}",
                                                req->methodString(),
                                                req->path(),
                                                req->query().empty() ? "" : "?" + req->query(),
                                                static_cast<int>(resp->getStatusCode()),
                                                seconds,
                                                resp->body().length(),
                                                req->versionString(),
                                                req->peerAddr().toIp(),
                                                req->attributes()->get<std::string>("request_id")
                                    )
                    );
                });


            // LogOrigin::info("HTTP Request", fmt::format("{} {:<7} {}  - Status: {}  - Time: {}ms\n\t└──Body: {}",

            // Register CORS pre-routing advice
            drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req,
                                                      drogon::AdviceCallback &&callback,
                                                      drogon::AdviceChainCallback &&chainCallback) {
                // Handle OPTIONS preflight
                if (req->method() == drogon::Options) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    resp->addHeader("Access-Control-Allow-Origin", "*");
                    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
                    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
                    resp->addHeader("Access-Control-Max-Age", "86400");
                    callback(resp);
                    return;
                }
                chainCallback();
            });

            // Register post-routing advice for CORS headers on all responses
            drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr &,
                                                        const drogon::HttpResponsePtr &resp) {
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
            });

            // Register default 404 handler
            auto notFoundResp = drogon::HttpResponse::newHttpResponse();
            notFoundResp->setStatusCode(drogon::k404NotFound);
            notFoundResp->setContentTypeString("application/json");
            notFoundResp->setBody(R"({"status":404,"error":"Not Found","data":{}})");
            drogon::app().setCustom404Page(notFoundResp);

            m_running.store(true);

            // Launch logging/browser in separate thread after listen starts
            std::thread notifier([this, host, port, launch_admin_setup]() -> void {
                // Wait a little for the server to be fully ready
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                auto endpoint = std::format("{}:{}", host, port);

                auto t_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                t_sink->set_level(spdlog::level::trace);
                t_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%-8l] %v");

                spdlog::logger logger("t_sink", {t_sink});
                logger.set_level(spdlog::level::trace);

                logger.info(
                    "Starting Servers: \n\t├── API Endpoints: http://{}/api/v1/ \n\t└── Admin Dashboard: http://{}/mb\n",
                    endpoint, endpoint);

                if (launch_admin_setup)
                    mApp.openBrowserOnStart();
            });

            notifier.detach();

            // drogon::app().run() blocks until quit() is called
            drogon::app().run();

            m_running.store(false);
            return true;
        } catch (const std::exception &e) {
            m_running.store(false);
            LogOrigin::critical("Server", fmt::format("Failed to start server: {}", e.what()));
        } catch (...) {
            m_running.store(false);
            LogOrigin::critical("Server", "Failed to start server: Unknown Error");
        }

        return false;
    }

    void Router::close() {
        // Stop SSE manager first before the router
        if (m_sseMgr->isRunning())
            m_sseMgr->stop();

        // Stop router and clear out objects
        if (m_running.load()) {
            drogon::app().quit();
            m_running.store(false);
            m_entityMap.clear();
            LogOrigin::info("Server", "HTTP Server Stopped.");
        }
    }

    bool Router::isRunning() const {
        return m_running.load();
    }

    SSEMgr &Router::sseMgr() const {
        return *m_sseMgr;
    }

    const json &Router::schemaCache(const std::string &table_name) const {
        // NOTE: returns a reference into the cache; the caller must not rely on
        // it remaining valid across a concurrent schema mutation. Currently
        // unused, kept for API completeness.
        std::shared_lock lock(m_entityMapMutex);
        if (!m_entityMap.contains(table_name)) {
            throw MantisException(404, "Entity schema for " + table_name + " was not found!");
        }

        return m_entityMap.at(table_name).schema();
    }

    bool Router::hasSchemaCache(const std::string &table_name) const {
        std::shared_lock lock(m_entityMapMutex);
        return m_entityMap.contains(table_name);
    }

    Entity Router::schemaCacheEntity(const std::string &table_name) const {
        std::shared_lock lock(m_entityMapMutex);
        if (!m_entityMap.contains(table_name)) {
            throw MantisException(404, "Entity schema for `" + table_name + "` was not found!");
        }

        // Returns a copy, so it stays valid after the lock is released.
        return Entity(m_entityMap.at(table_name));
    }

    void Router::addSchemaCache(const nlohmann::json &entity_schema) {
        std::unique_lock lock(m_entityMapMutex);
        addSchemaCacheLocked(entity_schema);
    }

    void Router::updateSchemaCache(const std::string &old_entity_name, const json &new_schema) {
        std::unique_lock lock(m_entityMapMutex);

        if (!m_entityMap.contains(old_entity_name))
            throw MantisException(404, "Cannot update, schema not found for entity " + old_entity_name);

        // Clean up old entity, then insert the new one, all under a single
        // exclusive lock so readers never observe the intermediate state.
        removeSchemaCacheLocked(old_entity_name);
        addSchemaCacheLocked(new_schema);
    }

    void Router::removeSchemaCache(const std::string &entity_name) const {
        std::unique_lock lock(m_entityMapMutex);
        removeSchemaCacheLocked(entity_name);
    }

    void Router::addSchemaCacheLocked(const nlohmann::json &entity_schema) const {
        auto entity_name = entity_schema.at("name").get<std::string>();
        if (m_entityMap.contains(entity_name)) {
            throw MantisException(500, "An entity exists with given entity_name");
        }

        // Create entity and cache it, bound to this application. Unified entity
        // routes resolve entities dynamically.
        // auto entity = Entity(mApp, entity_schema);
        m_entityMap.try_emplace(entity_name, Entity(mApp, entity_schema));
    }

    void Router::removeSchemaCacheLocked(const std::string &entity_name) const {
        if (!m_entityMap.contains(entity_name)) {
            throw MantisException(404, "Could not find EntitySchema for " + entity_name);
        }

        m_entityMap.erase(entity_name);
    }

    void Router::registerAuthRoutes() {
        const Middlewares authEntityMiddleware = {resolveAuthEntity()};
        const Middlewares authLoginMiddleware = {resolveAuthEntity(), rateLimit(5, 60, false)};

        Post("/api/v1/auth/:entity_name/login", handleAuthLogin(), authLoginMiddleware);
        Post("/api/v1/auth/:entity_name/refresh", handleAuthRefresh(), authEntityMiddleware);
        Post("/api/v1/auth/:entity_name/logout", handleAuthLogout(), authEntityMiddleware);
    }

    void Router::registerSchemaRoutes() {
        const Middlewares adminAuth = {requireAdminAuth()};
        const Middlewares schemaItemMiddleware = {requireAdminAuth(), resolveSchema()};

        Get("/api/v1/schemas", schemaGetManyHandler(), adminAuth);
        Post("/api/v1/schemas", schemaPostHandler(), adminAuth);
        Get("/api/v1/schemas/:schema_name_or_id", schemaGetOneHandler(), schemaItemMiddleware);
        Patch("/api/v1/schemas/:schema_name_or_id", schemaPatchHandler(), schemaItemMiddleware);
        Delete("/api/v1/schemas/:schema_name_or_id", schemaDeleteHandler(), schemaItemMiddleware);
    }

    void Router::registerEntityRoutes() {
        const Middlewares readMiddleware = {resolveEntity(), hasEntityAccess()};
        const Middlewares mutateMiddleware = {resolveEntity(), rejectViewMutations(), hasEntityAccess()};

        Get("/api/v1/entities/:entity_name", entityGetManyHandler(), readMiddleware);
        Get("/api/v1/entities/:entity_name/:id", entityGetOneHandler(), readMiddleware);
        Post("/api/v1/entities/:entity_name", entityPostHandler(), mutateMiddleware);
        Patch("/api/v1/entities/:entity_name/:id", entityPatchHandler(), mutateMiddleware);
        Delete("/api/v1/entities/:entity_name/:id", entityDeleteHandler(), mutateMiddleware);
    }

    void Router::generateMiscEndpoints() {
        auto &router = mApp.router();

        // Admin dashboard route - use Drogon regex handler
        auto adminHandler = handleAdminDashboardRoute();
        drogon::app().registerHandlerViaRegex(
            R"(/mb(/.*)?)",
            [adminHandler, this](const drogon::HttpRequestPtr &req,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
                MantisRequest ma_req{mApp, req};
                MantisResponse ma_res{};
                adminHandler(ma_req, ma_res);
                callback(ma_res.drogonResponse());
            },
            {drogon::Get});

        router.Get("/api/v1/health", healthCheckHandler());

        // /api/v1/sys/*
        router.Get("/api/v1/sys/logs", handleLogs(), {requireAdminAuth()});
        router.Post("/api/v1/sys/admins/login", handleAdminLogin(), {rateLimit(5, 60, false)});
        router.Post("/api/v1/sys/admins/refresh", handleAuthRefresh());
        router.Post("/api/v1/sys/admins/logout", handleAuthLogout());
        router.Post("/api/v1/sys/admins/setup", handleSetupAdmin(), {rateLimit(3, 3600, false)});

        // /api/v1/auth/<entity>/*
        registerAuthRoutes();

        // /api/v1/files/*
        router.Get("/api/v1/files/:entity/:file", fileServingHandler());

        router.Get("/api/v1/sys/settings/config", [](const MantisRequest &, const MantisResponse &res) {
            res.sendJSON(200, {{"data", {}}, {"status", 200}, {"error", nullptr}});
        });

        router.Post("/api/v1/sys/settings/config", [](const MantisRequest &, const MantisResponse &res) {
            res.sendJSON(200, {{"data", {}}, {"status", 200}, {"error", nullptr}});
        });

        router.Patch("/api/v1/sys/settings/config", [](const MantisRequest &, const MantisResponse &res) {
            res.sendJSON(200, {{"data", {}}, {"status", 200}, {"error", nullptr}});
        });


        m_sseMgr->createRoutes();

        registerSchemaRoutes();
        registerEntityRoutes();
        registerAdminEntityRoutes();
        registerApiKeyRoutes();
        registerOAuthRoutes();

        // Static file serving: register a catch-all for the public directory
        const auto publicDir = mApp.publicDir();
        if (fs::exists(publicDir)) {
            drogon::app().setDocumentRoot(publicDir);
        }
    }

    std::function<void(const MantisRequest &, MantisResponse &)> Router::handleAdminDashboardRoute() {
        return [](const MantisRequest &req, MantisResponse &res) {
            try {
                const auto fs = cmrc::mantis::get_filesystem();

                // Extract path after /mb
                std::string full_path = req.getPath();
                std::string path;
                if (full_path.size() > 3) {
                    path = full_path.substr(3); // strip "/mb"
                }

                // Normalize the path
                if (path.empty() || path == "/") {
                    path = "/public/index.html";
                } else {
                    path = std::format("/public{}", path);
                }

                if (!fs.exists(path)) {
                    LogOrigin::trace("Path Missing", fmt::format("{} path does not exists", path));

                    // fallback to index.html for React routes
                    path = "/public/index.html";
                }

                try {
                    const auto file = fs.open(path);
                    const auto mime = Router::getMimeType(path);
                    res.setContent(file.begin(), file.size(), mime);
                    res.setStatus(200);
                } catch (const std::exception &e) {
                    const auto file = fs.open("/public/404.html");
                    const auto mime = Router::getMimeType("404.html");

                    res.setContent(file.begin(), file.size(), mime);
                    res.setStatus(404);
                    LogOrigin::critical("Admin Response Error",
                                        fmt::format("Error processing /admin response: {}", e.what()));
                }
            } catch (const std::exception &e) {
                res.setStatus(500);
                res.setReason(e.what());
                LogOrigin::critical("Admin Request Error",
                                    fmt::format("Error processing /admin request: {}", e.what()));
            }
        };
    }

    std::function<void(const MantisRequest &, MantisResponse &)> Router::fileServingHandler() {
        LogOrigin::trace("Endpoint Registration", "Registering /api/v1/files/:entity/:file GET endpoint ...");
        return [](const MantisRequest &req, MantisResponse &res) {
            const auto table_name = req.getPathParamValue("entity");
            const auto file_name = req.getPathParamValue("file");

            if (!EntitySchema::isValidEntityName(table_name)) {
                json response;
                response["error"] = std::format("Invalid entity name `{}`", table_name);
                response["status"] = 400;
                response["data"] = json::object();

                res.sendJSON(400, response);
                return;
            }

            if (table_name.empty() || file_name.empty()) {
                json response;
                response["error"] = "Both entity name and file name are required!";
                response["status"] = 400;
                response["data"] = json::object();

                res.sendJSON(400, response);
                return;
            }

            if (const auto path_opt = Files::getFilePath(table_name, file_name);
                path_opt.has_value()) {
                // Return requested file
                res.setStatus(200);
                res.setFileContent(path_opt.value());
                return;
            }

            json response;
            response["error"] = "File not found!";
            response["status"] = 404;
            response["data"] = json::object();

            res.sendJSON(404, response);
        };
    }

    std::function<void(const MantisRequest &, MantisResponse &)> Router::healthCheckHandler() {
        return [](const MantisRequest &, const MantisResponse &res) {
            res.setHeader("Cache-Control", "no-cache");
            res.send(200, R"({"status": "OK"})", "application/json");
        };
    }

    std::string Router::getMimeType(const std::string &path) {
        if (path.ends_with(".js")) return "application/javascript";
        if (path.ends_with(".css")) return "text/css";
        if (path.ends_with(".html")) return "text/html";
        if (path.ends_with(".json")) return "application/json";
        if (path.ends_with(".png")) return "image/png";
        if (path.ends_with(".svg")) return "image/svg+xml";
        return "application/octet-stream";
    }

    std::function<void(const MantisRequest &, MantisResponse &)> Router::handleLogs() {
        return [&](const MantisRequest &req, MantisResponse &res) {
            try {
                if (!Logger::isDbInitialized) {
                    json response;
                    response["error"] = "Log database not initialized";
                    response["status"] = 503;
                    response["data"] = json::object();
                    res.sendJSON(503, response);
                    return;
                }

                // Parse query parameters
                int limit = 50;
                std::string after;
                std::string level_filter;
                std::string min_level_filter;
                std::string search_filter;
                std::string start_date;
                std::string end_date;
                std::string sort_by = "timestamp";
                std::string sort_order = "desc";

                if (req.hasQueryParam("after")) {
                    after = req.getQueryParamValue("after");
                }

                if (req.hasQueryParam("limit")) {
                    try {
                        limit = std::stoi(req.getQueryParamValue("limit"));
                        if (limit < 1) limit = 1;
                        if (limit > 1000) limit = 1000;
                    } catch (...) {
                        limit = 50;
                    }
                }

                if (req.hasQueryParam("level")) {
                    level_filter = req.getQueryParamValue("level");

                    if (level_filter != "trace" && level_filter != "debug" &&
                        level_filter != "info" && level_filter != "warn" &&
                        level_filter != "critical") {
                        level_filter.clear();
                    }
                }

                if (req.hasQueryParam("min_level")) {
                    min_level_filter = req.getQueryParamValue("min_level");

                    if (min_level_filter != "trace" && min_level_filter != "debug" &&
                        min_level_filter != "info" && min_level_filter != "warn" &&
                        min_level_filter != "critical") {
                        min_level_filter.clear();
                    }
                }

                if (req.hasQueryParam("search")) {
                    search_filter = req.getQueryParamValue("search");
                }

                if (req.hasQueryParam("start_date")) {
                    start_date = req.getQueryParamValue("start_date");
                }
                if (req.hasQueryParam("end_date")) {
                    end_date = req.getQueryParamValue("end_date");
                }

                if (req.hasQueryParam("sort_by")) {
                    std::string sort_param = req.getQueryParamValue("sort_by");
                    if (sort_param == "level" || sort_param == "origin" || sort_param == "message" ||
                        sort_param == "timestamp" || sort_param == "created_at") {
                        sort_by = sort_param;
                    }
                }

                if (req.hasQueryParam("sort_order")) {
                    if (std::string order = req.getQueryParamValue("sort_order"); order == "asc" || order == "desc") {
                        sort_order = order;
                    }
                }

                if (!min_level_filter.empty()) level_filter = ">" + min_level_filter;

                // Get log database instance
                auto &logsDb = req.mApp().logs().logsDb();
                json result = logsDb.getLogs(after, limit, level_filter,
                                             search_filter, start_date, end_date,
                                             sort_by, sort_order);

                json response;
                response["error"] = "";
                response["status"] = 200;
                response["data"] = result;
                res.sendJSON(200, result);
            } catch (const MantisException &e) {
                json response;
                response["error"] = e.what();
                response["status"] = 500;
                response["data"] = json::object();
                res.sendJSON(500, response);
            } catch (const std::exception &e) {
                json response;
                response["error"] = std::string("Failed to fetch logs: ") + e.what();
                response["status"] = 500;
                response["data"] = json::object();
                res.sendJSON(500, response);
            }
        };
    }
}
