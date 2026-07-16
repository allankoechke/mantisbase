#ifndef MB_ROUTER_H
#define MB_ROUTER_H

#include <memory>
#include <vector>
#include <atomic>
#include <shared_mutex>
#include <nlohmann/json.hpp>

#include "route_registry.h"
#include "models/entity.h"
#include "../utils/utils.h"
#include "types.h"

namespace mb {
    class SSEMgr;

    class Router {
    public:
        explicit Router(const MantisBase& app);
        ~Router();

        bool init();
        bool listen();
        void close();

        SSEMgr& sseMgr() const;

        void Get(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});
        void Post(const std::string &path, const HandlerWithContentReaderFn &handler, const Middlewares &middlewares = {});
        void Post(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});
        void Patch(const std::string &path, const HandlerWithContentReaderFn &handler, const Middlewares &middlewares = {});
        void Patch(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});
        void Delete(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        const json &schemaCache(const std::string &table_name) const;
        bool hasSchemaCache(const std::string &table_name) const;
        Entity schemaCacheEntity(const std::string &table_name) const;
        void addSchemaCache(const nlohmann::json &entity_schema);
        void updateSchemaCache(const std::string &old_entity_name, const json &new_schema);
        void removeSchemaCache(const std::string &entity_name);

        void addSchemaCacheLocked(const nlohmann::json &entity_schema);

        void removeSchemaCacheLocked(const std::string &entity_name);

        bool isRunning() const;

        const std::vector<MiddlewareFn> &preRoutingMiddlewares() const { return m_preRoutingMiddlewares; }

    private:
        void registerDrogonHandler(const std::string &method, const std::string &path);
        void registerDrogonHandlerWithReader(const std::string &method, const std::string &path);

        static std::string convertPathToDrogon(const std::string &httplib_path);
        static std::vector<std::string> extractParamNames(const std::string &httplib_path);

        void executeMiddlewareChain(MantisRequest &req, MantisResponse &res, const RouteHandler *route) const;

        void generateMiscEndpoints();
        void registerEntityRoutes();
        void registerSchemaRoutes();
        void registerAuthRoutes();
        void registerApiKeyRoutes();
        void registerOAuthRoutes();

        static std::string getMimeType(const std::string &path);

        static std::function<void(const MantisRequest &, MantisResponse &)> handleAdminDashboardRoute();
        static std::function<void(const MantisRequest &, MantisResponse &)> fileServingHandler();
        static std::function<void(const MantisRequest &, MantisResponse &)> healthCheckHandler();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogin();
        std::function<void(MantisRequest &, MantisResponse &)> handleAdminLogin();
        std::function<void(MantisRequest &, MantisResponse &)> handleAuthRefresh();
        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogout();
        std::function<void(MantisRequest &, MantisResponse &)> handleSetupAdmin();

        static std::function<void(const MantisRequest &, MantisResponse &)> handleLogs();

        const MantisBase &mApp;
        RouteRegistry m_routeRegistry;
        std::unique_ptr<SSEMgr> m_sseMgr;
        std::vector<MiddlewareFn> m_preRoutingMiddlewares;
        std::vector<HandlerFn> m_postRoutingMiddlewares;

        /// Entity schema cache. Read on every request by the httplib worker
        /// threads and mutated at runtime by the schema CRUD endpoints, so all
        /// access must be synchronized via m_entityMapMutex.
        mutable std::unordered_map<std::string, Entity> m_entityMap;
        std::atomic<bool> m_running{false};
        mutable std::shared_mutex m_entityMapMutex;
    };
} // mb

#endif // MB_ROUTER_H
