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

// For thread logger
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <spdlog/sinks/ansicolor_sink.h>

#include "../include/mantisbase/core/exceptions.h"
#include "../include/mantisbase/core/middlewares.h"
#include "../include/mantisbase/core/models/entity_schema.h"

// Declare a mantis namespace for the embedded FS
CMRC_DECLARE(mantis);

namespace mantis {
    Router::Router()
        : mApp(MantisBase::instance()),
          m_entitySchema(std::make_unique<EntitySchema>()) {
        // Let's fix timing initialization, set the start time to current time
        svr.set_pre_routing_handler(preRoutingHandler());

        // Add CORS headers to all responses
        svr.set_post_routing_handler(postRoutingHandler());

        svr.set_logger(routingLogger());

        // Handle preflight OPTIONS requests
        svr.Options(".*", optionsHandler());

        // Set Error Handler
        svr.set_error_handler(routingErrorHandler());

        // Add global middlewares to work across all routes
        m_preRoutingMiddlewares.push_back(getAuthToken()); // Get auth token from the header
        m_preRoutingMiddlewares.push_back(hydrateContextData()); // Fill request context with necessary data
    }

    Router::~Router() {
        if (svr.is_running())
            svr.stop();
    }

    bool Router::initialize() {
        {
            const auto sql = mApp.db().session();
            const soci::rowset rows = (sql->prepare << "SELECT schema FROM mb_tables");

            for (const auto &row: rows) {
                const auto schema = row.get<nlohmann::json>("schema");

                // Create entity based on the schema
                Entity entity{schema};

                // Create routes based on the entity type
                if (entity.hasApi()) entity.createEntityRoutes();

                // Store this object to keep alive function pointers
                // if not, possible access violation error
                m_entityMap.emplace(entity.name(), std::move(entity));
            }
        }

        // Add admin routes
        EntitySchema admin_schema{"mb_admins", "auth"};
        admin_schema.removeField("name");
        admin_schema.setSystem(true);
        auto admin_entity = admin_schema.toEntity();
        admin_entity.createEntityRoutes();
        m_entityMap.emplace(admin_entity.name(), std::move(admin_entity));

        // Service Schema [No routes]
        EntitySchema service_schema{"mb_service_acc", "base"};
        service_schema.setHasApi(false);
        service_schema.setSystem(true);
        auto service_entity = service_schema.toEntity();
        m_entityMap.emplace(service_entity.name(), std::move(service_entity));

        // Misc Endpoints [admin, auth, etc]
        generateMiscEndpoints();

        return true;
    }

    bool Router::listen() {
        try {
            // Check if server can bind to port before launching
            if (!svr.is_valid()) {
                logger::critical("Server is not valid. Maybe port is in use or permissions issue.\n");
                return false;
            }

            const auto host = mApp.host();
            const auto port = mApp.port();

            // Launch logging/browser in separate thread after listen starts
            std::thread notifier([host, port]() -> void {
                // Wait a little for the server to be fully ready
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                auto endpoint = std::format("{}:{}", host, port);

                auto t_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                t_sink->set_level(spdlog::level::trace);
                t_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%-8l] %v");

                spdlog::logger logger("t_sink", {t_sink});
                logger.set_level(spdlog::level::trace);

                logger.info(
                    "Starting Servers: \n\t├── API Endpoints: http://{}/api/v1/ \n\t└── Admin Dashboard: http://{}/mb-admin\n",
                    endpoint, endpoint);

                MantisBase::instance().openBrowserOnStart();
            });

            if (!svr.listen(host, port)) {
                logger::critical("Error: Failed to bind to {}:{}", host, port);
                notifier.join();
                return false;
            }

            notifier.join();
            return true;
        } catch (const std::exception &e) {
            logger::critical("Failed to start server: {}", e.what());
        } catch (...) {
            logger::critical("Failed to start server: Unknown Error");
        }

        return false;
    }

    void Router::close() {
        if (svr.is_running()) {
            svr.stop();
            m_entityMap.clear();
            logger::info("HTTP Server Stopped.\n\t ...");
        }
    }

    httplib::Server &Router::server() {
        return svr;
    }

    void Router::Get(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares) {
        m_routeRegistry.add("GET", path, handler, middlewares);
        globalRouteHandler("GET", path);
    }

    void Router::Post(const std::string &path, const HandlerWithContentReaderFn &handler,
                      const Middlewares &middlewares) {
        m_routeRegistry.add("POST", path, handler, middlewares);
        globalRouteHandlerWithReader("POST", path);
    }

    void Router::Post(const std::string &path, const HandlerFn &handler,
                      const Middlewares &middlewares) {
        m_routeRegistry.add("POST", path, handler, middlewares);
        globalRouteHandler("POST", path);
    }

    void Router::Patch(const std::string &path, const HandlerWithContentReaderFn &handler,
                       const Middlewares &middlewares) {
        m_routeRegistry.add("PATCH", path, handler, middlewares);
        globalRouteHandlerWithReader("PATCH", path);
    }

    void Router::Patch(const std::string &path, const HandlerFn &handler,
                       const Middlewares &middlewares) {
        m_routeRegistry.add("PATCH", path, handler, middlewares);
        globalRouteHandler("PATCH", path);
    }

    void Router::Delete(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares) {
        m_routeRegistry.add("DELETE", path, handler, middlewares);
        globalRouteHandler("DELETE", path);
    }

    const json &Router::schemaCache(const std::string &table_name) const {
        if (!m_entityMap.contains(table_name)) {
            throw MantisException(404, "Entity schema for " + table_name + " was not found!");
        }

        return m_entityMap.at(table_name).schema();
    }

    Entity Router::schemaCacheEntity(const std::string &table_name) const {
        if (!m_entityMap.contains(table_name)) {
            throw MantisException(404, "Entity schema for `" + table_name + "` was not found!");
        }

        return m_entityMap.at(table_name);
    }

    void Router::addSchemaCache(const nlohmann::json &entity_schema) {
        const auto entity_name = entity_schema.at("name").get<std::string>();
        if (m_entityMap.contains(entity_name)) {
            throw MantisException(500, "An entity exists with given entity_name");
        }

        // Create entity and its routes
        auto entity = Entity(entity_schema);
        entity.createEntityRoutes();
        m_entityMap.insert_or_assign(entity_name, std::move(entity));
    }

    void Router::updateSchemaCache(const std::string &old_entity_name, const json &new_schema) {
        if (!m_entityMap.contains(old_entity_name))
            throw MantisException(404, "Cannot update, schema not found for entity " + old_entity_name);

        // Clean up old entity route
        removeSchemaCache(old_entity_name);

        // Add new route
        addSchemaCache(new_schema);
    }

    void Router::removeSchemaCache(const std::string &entity_name) {
        // Let's find and remove existing object
        if (!m_entityMap.contains(entity_name)) {
            throw MantisException(404, "Could not find EntitySchema for " + entity_name);
        }

        // Get cached entity
        const Entity &entity = m_entityMap.at(entity_name);

        // Also, check if we have defined some routes for this one ...
        const auto basePath = "/api/v1/entities/" + entity_name;
        m_routeRegistry.remove("GET", basePath);
        m_routeRegistry.remove("GET", basePath + "/:id");

        if (entity.type() != "view") {
            m_routeRegistry.remove("POST", basePath);
            m_routeRegistry.remove("PATCH", basePath + "/:id");
            m_routeRegistry.remove("DELETE", basePath + "/:id");
        }

        // Remove Entity instance for the cache
        m_entityMap.erase(entity_name);
    }

    void Router::globalRouteHandler(const std::string &method, const std::string &path) {
        const std::function handlerFunc = [this, method, path](const httplib::Request &req, httplib::Response &res) {
            MantisRequest ma_req{req};
            MantisResponse ma_res{res};

            const auto route = m_routeRegistry.find(method, path);
            if (!route) {
                json response;
                response["status"] = 404;
                response["error"] = std::format("{} {} Route Not Found", method, path);
                response["data"] = json::object();

                ma_res.sendJSON(404, response);
                return;
            }

            // First, execute global middlewares
            for (const auto &g_mw: m_preRoutingMiddlewares) {
                if (g_mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Secondly, execute route specific middlewares
            for (const auto &mw: route->middlewares) {
                if (mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Finally, execute the handler function
            if (const auto func = std::get_if<HandlerFn>(&route->handler)) {
                (*func)(ma_req, ma_res);
            }

            // Any post routing checks?
            for (const auto &p_mw: m_postRoutingMiddlewares) {
                p_mw(ma_req, ma_res); // Execute all, no return types
            }
        };

        if (method == "GET") {
            svr.Get(path, handlerFunc);
        } else if (method == "PATCH") {
            svr.Patch(path, handlerFunc);
        } else if (method == "POST") {
            svr.Post(path, handlerFunc);
        } else if (method == "DELETE") {
            svr.Delete(path, handlerFunc);
        } else {
            throw MantisException(500, "Router method `" + method + "` is not supported!");
        }
    }

    void Router::globalRouteHandlerWithReader(const std::string &method, const std::string &path) {
        const std::function handlerFuncWithContentReader = [this, method, path](
            const httplib::Request &req, httplib::Response &res, const httplib::ContentReader &cr) {
            MantisRequest ma_req{req};
            MantisResponse ma_res{res};
            MantisContentReader ma_cr{cr, ma_req};

            const auto route = m_routeRegistry.find(method, path);
            if (!route) {
                ma_res.sendJSON(404, {
                                    {"status", 404},
                                    {"error", std::format("{} {} Route Not Found", method, path)},
                                    {"data", json::object()}
                                });
                return;
            }

            // First, execute global middlewares
            for (const auto &g_mw: m_preRoutingMiddlewares) {
                if (g_mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Secondly, execute route specific middlewares
            for (const auto &mw: route->middlewares) {
                if (mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Finally, execute the handler function
            if (const auto func = std::get_if<HandlerWithContentReaderFn>(&route->handler)) {
                (*func)(ma_req, ma_res, ma_cr);
            }

            // Any post routing checks?
            for (const auto &p_mw: m_postRoutingMiddlewares) {
                p_mw(ma_req, ma_res); // Execute all, no return types
            }
        };

        if (method == "PATCH") {
            svr.Patch(path, handlerFuncWithContentReader);
        } else if (method == "POST") {
            svr.Post(path, handlerFuncWithContentReader);
        } else {
            throw MantisException(500, "Router method `" + method + "` is not supported!");
        }
    }

    void Router::generateMiscEndpoints() {
        auto &router = mApp.router();
        router.Get("/api/v1/health", healthCheckHandler());
        router.Get("/api/files/:entity/:file", fileServingHandler());
        router.Get(R"(/mb-admin(.*))", handleAdminDashboardRoute());

        // Systemwide auth endpoints
        router.Post("/api/v1/auth/login", handleAuthLogin());
        router.Post("/api/v1/auth/refresh", handleAuthRefresh());
        router.Post("/api/v1/auth/logout", handleAuthLogout());
        router.Post("/api/v1/auth/setup/admin", handleSetupAdmin());

        // Add entity schema routes
        // GET|POST|PATCH|DELETE `/api/v1/schemas*`
        m_entitySchema->createEntityRoutes();

        // Add /public static file serving directory
        if (const auto mount_ok = svr.set_mount_point("/", mApp.publicDir()); !mount_ok) {
            logger::critical("Failed to setup mount point directory for '/' at '{}'",
                             mApp.publicDir());
        }
    }

    std::function<void(const MantisRequest &, MantisResponse &)> Router::handleAdminDashboardRoute() const {
        return [](const MantisRequest &req, MantisResponse &res) {
            try {
                const auto fs = cmrc::mantis::get_filesystem();
                std::string path = req.matches()[1];

                // Normalize the path
                if (path.empty() || path == "/") {
                    path = "/public/index.html";
                } else {
                    path = std::format("/public{}", path);
                }

                if (!fs.exists(path)) {
                    logger::trace("{} path does not exists", path);

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
                    logger::critical("Error processing /admin response: {}", e.what());
                }
            } catch (const std::exception &e) {
                res.setStatus(500);
                logger::critical("Error processing /admin request: {}", e.what());
            }
        };
    }

    std::function<void(const MantisRequest &, MantisResponse &)> Router::fileServingHandler() {
        logger::trace("Registering /api/files/:entity/:file GET endpoint ...");
        return [](const MantisRequest &req, MantisResponse &res) {
            std::cout << "fileServingHandler()" << std::endl;
            const auto table_name = req.getPathParamValue("entity");
            const auto file_name = req.getPathParamValue("file");

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
}
