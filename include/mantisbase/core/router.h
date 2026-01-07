/**
 * @brief router.h
 * @brief Router file for higher level route registration and removal *
 */

#ifndef MANTIS_SERVER_H
#define MANTIS_SERVER_H

#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
#include <httplib.h>

#include "route_registry.h"
#include "models/entity.h"
#include "../utils/utils.h"
#include  "types.h"

namespace mb {
    /**
     * @brief HTTP router for managing routes and request handling.
     *
     * Router provides methods to register HTTP routes with handlers and middlewares,
     * manages entity schema cache, and handles request routing.
     *
     * @code
     * // Register a GET route
     * router.Get("/users", [](MantisRequest& req, MantisResponse& res) {
     *     res.sendJSON(200, {{"users", "data"}});
     * });
     *
     * // Register a POST route with middleware
     * router.Post("/posts", handler, {requireAdminAuth()});
     * @endcode
     */
    class Router {
    public:
        /**
         * @brief Construct router instance.
         */
        Router();

        /**
         * @brief Destructor.
         */
        ~Router();

        /**
         * @brief Initialize router: create system tables and admin routes.
         * @return true if initialization successful
         */
        bool initialize();

        /**
         * @brief Start HTTP server and begin listening for connections.
         * @return true if server started successfully
         */
        bool listen();

        /**
         * @brief Close HTTP server and stop listening.
         */
        void close();

        /**
         * @brief Get underlying httplib::Server instance.
         * @return Reference to HTTP server
         */
        httplib::Server &server();

        // ----------- HTTP METHODS ----------- //
        /**
         * @brief Register GET route.
         * @param path Route path (supports path parameters like /:id)
         * @param handler Request handler function
         * @param middlewares Optional middleware functions
         * @code
         * router.Get("/users/:id", [](MantisRequest& req, MantisResponse& res) {
         *     std::string id = req.getPathParamValue("id");
         *     // ... handle request
         * });
         * @endcode
         */
        void Get(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        /**
         * @brief Register POST route with content reader (for file uploads).
         * @param path Route path
         * @param handler Handler with content reader for multipart/form-data
         * @param middlewares Optional middleware functions
         */
        void Post(const std::string &path, const HandlerWithContentReaderFn &handler,
                  const Middlewares &middlewares = {});

        /**
         * @brief Register POST route.
         * @param path Route path
         * @param handler Request handler function
         * @param middlewares Optional middleware functions
         */
        void Post(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        /**
         * @brief Register PATCH route with content reader (for file uploads).
         * @param path Route path
         * @param handler Handler with content reader
         * @param middlewares Optional middleware functions
         */
        void Patch(const std::string &path, const HandlerWithContentReaderFn &handler,
                   const Middlewares &middlewares = {});

        /**
         * @brief Register PATCH route.
         * @param path Route path
         * @param handler Request handler function
         * @param middlewares Optional middleware functions
         */
        void Patch(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        /**
         * @brief Register DELETE route.
         * @param path Route path
         * @param handler Request handler function
         * @param middlewares Optional middleware functions
         */
        void Delete(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        // ----------- SCHEMA CACHE METHODS ----------- //
        /**
         * @brief Get cached schema JSON by table name.
         * @param table_name Table name to lookup
         * @return Reference to cached schema JSON
         */
        const json &schemaCache(const std::string &table_name) const;

        /**
         * @brief Check whether schema cache for given name exists.
         * @param table_name Table name to lookup
         * @return true if found, false otherwise.
         */
        bool hasSchemaCache(const std::string &table_name) const;

        /**
         * @brief Get cached entity by table name.
         * @param table_name Table name to lookup
         * @return Entity instance from cache
         */
        Entity schemaCacheEntity(const std::string &table_name) const;

        /**
         * @brief Add schema to cache.
         * @param entity_schema Schema JSON to cache
         */
        void addSchemaCache(const nlohmann::json &entity_schema);

        /**
         * @brief Update cached schema.
         * @param old_entity_name Old table name
         * @param new_schema Updated schema JSON
         */
        void updateSchemaCache(const std::string &old_entity_name, const json &new_schema);

        /**
         * @brief Remove schema from cache.
         * @param entity_name Table name to remove
         */
        void removeSchemaCache(const std::string &entity_name);

        // ----------- UTILS METHODS ----------- //
        /**
         * @brief Decompress response body based on encoding.
         * @param body Compressed body data
         * @param encoding Encoding type (e.g., "gzip", "deflate")
         * @return Decompressed body string
         */
        std::string decompressResponseBody(const std::string &body, const std::string &encoding);

    private:
        void globalRouteHandler(const std::string &method, const std::string &path);

        void globalRouteHandlerWithReader(const std::string &method, const std::string &path);

        void generateMiscEndpoints();

        static std::string getMimeType(const std::string &path);

        // ----------- REQ/RES METHODS ----------- //
        static std::function<void(const MantisRequest &, MantisResponse &)> handleAdminDashboardRoute() ;

        static std::function<void(const MantisRequest &, MantisResponse &)> fileServingHandler();

        static std::function<void(const MantisRequest &, MantisResponse &)> healthCheckHandler();

        std::function<HandlerResponse(const httplib::Request &, httplib::Response &)> preRoutingHandler();

        std::function<void(const httplib::Request &, httplib::Response &)> postRoutingHandler();

        std::function<void(const httplib::Request &, httplib::Response &)> optionsHandler();

        std::function<void(const httplib::Request &, const httplib::Response &)> routingLogger();

        std::function<void(const httplib::Request &, httplib::Response &)> routingErrorHandler();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogin();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthRefresh();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogout();

        std::function<void(MantisRequest &, MantisResponse &)> handleSetupAdmin();

        static std::function<void(const MantisRequest &, MantisResponse &)> handleLogs();

        // Member Variables
        MantisBase &mApp;
        httplib::Server svr;
        RouteRegistry m_routeRegistry;
        // std::vector<nlohmann::json> m_schemas;
        std::vector<MiddlewareFn> m_preRoutingMiddlewares;
        std::vector<HandlerFn> m_postRoutingMiddlewares;
        std::unique_ptr<EntitySchema> m_entitySchema;
        std::unordered_map<std::string, Entity> m_entityMap;
    };
} // mb

#endif // MANTIS_SERVER_H
