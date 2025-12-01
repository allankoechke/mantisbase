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

namespace mantis {
    /**
     * @brief Router class allows for managing routes as well as acting as a top-wrapper on the HttpUnit.
     */
    class Router {
    public:
        Router();

        ~Router();

        /// Initialize the router instance creating tables and admin routes.
        bool initialize();

        /// Bind to a port and start listening for new connections.
        /// Calls @see HttpUnit::listen() under the hood.
        bool listen();

        /// Close the HTTP Server connection
        /// Calls @see HttpUnit::close()
        void close();

        httplib::Server &server();

        // ----------- HTTP METHODS ----------- //
        void Get(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        void Post(const std::string &path, const HandlerWithContentReaderFn &handler, const Middlewares &middlewares = {});

        void Post(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        void Patch(const std::string &path, const HandlerWithContentReaderFn &handler, const Middlewares &middlewares = {});

        void Patch(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        void Delete(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        // ----------- SCHEMA CACHE METHODS ----------- //
        const json &schemaCache(const std::string &table_name) const;

        Entity schemaCacheEntity(const std::string &table_name) const;

        void addSchemaCache(const nlohmann::json &entity_schema);

        void updateSchemaCache(const std::string &old_entity_name, const json &new_schema);

        void removeSchemaCache(const std::string &table_name);

        // ----------- UTILS METHODS ----------- //
        std::string decompressResponseBody(const std::string &body, const std::string &encoding);

    private:
        void globalRouteHandler(const std::string &method, const std::string &path);

        void globalRouteHandlerWithReader(const std::string &method, const std::string &path);

        void generateMiscEndpoints();

        static std::string getMimeType(const std::string &path);

        // ----------- REQ/RES METHODS ----------- //
        std::function<void(const MantisRequest &, MantisResponse &)> handleAdminDashboardRoute() const;

        static std::function<void(const MantisRequest &, MantisResponse &)> fileServingHandler() ;

        static std::function<void(const MantisRequest &, MantisResponse &)> healthCheckHandler() ;

        std::function<HandlerResponse(const httplib::Request &, httplib::Response &)> preRoutingHandler();

        std::function<void(const httplib::Request &, httplib::Response &)> postRoutingHandler();

        std::function<void(const httplib::Request &, httplib::Response &)> optionsHandler();

        std::function<void(const httplib::Request &, const httplib::Response &)> routingLogger();

        std::function<void(const httplib::Request &, httplib::Response &)> routingErrorHandler();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogin();
        std::function<void(MantisRequest &, MantisResponse &)> handleAuthRefresh();
        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogout();


        MantisBase& mApp;
        httplib::Server svr;
        std::unique_ptr<EntitySchema> m_entitySchema;
        RouteRegistry m_routeRegistry;
        std::vector<MiddlewareFn> m_preRoutingMiddlewares;
        std::vector<HandlerFn> m_postRoutingMiddlewares;
        std::vector<nlohmann::json> m_schemas;
        std::unordered_map<std::string, Entity> m_entityMap;
    };
}

#endif // MANTIS_SERVER_H
