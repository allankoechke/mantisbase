/**
 * @file router.h
 * @brief HTTP router: route registration, schema cache, CORS, and built-in API endpoints.
 *
 * Bridges MantisBase handler/middleware chains (@ref RouteRegistry) to Drogon's HTTP server.
 * Also owns @ref SSEMgr (SSE + WebSocket realtime) and the entity schema cache used on
 * every CRUD request.
 */

#ifndef MB_ROUTER_H
#define MB_ROUTER_H

#include <memory>
#include <vector>
#include <set>
#include <atomic>
#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <drogon/utils/HttpConstraint.h>

#include "route_registry.h"
#include "models/entity.h"
#include "../utils/utils.h"
#include "types.h"
#include "drogon/drogon_callbacks.h"
#include "mantisbase/utils/snowflake.hpp"

#ifdef MB_SCRIPTING_ENABLED
#include <dukglue/dukvalue.h>
struct duk_hthread;
typedef struct duk_hthread duk_context;
#endif

namespace mb {
    class SSEMgr;

    /**
     * @brief Central HTTP router and schema cache for a @ref MantisBase instance.
     */
    class MANTISBASE_API Router : public IMantisBase {
    public:
        explicit Router(const MantisBase &app);

        ~Router();

        /** Initialize Drogon, register routes, and prepare the schema cache. */
        bool init();

        /** Start listening on the configured host/port. */
        bool listen();

        /** Stop the HTTP server and realtime subsystems. */
        void close();

        /** @return SSE/WebSocket realtime manager. */
        SSEMgr &sseMgr() const;

        void Get(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        void Post(const std::string &path, const HandlerWithContentReaderFn &handler,
                  const Middlewares &middlewares = {});

        void Post(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        void Patch(const std::string &path, const HandlerWithContentReaderFn &handler,
                   const Middlewares &middlewares = {});

        void Patch(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        void Delete(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares = {});

        /** Create a redirect path with the given status code for redirect
         *
         * @param initialPath Existing path to be redirected elsewhere (/some/old/path)
         * @param destinationPath Destination path, either relative (/some/path) or absolute URL (http(s)://some.domain/path)
         * @param redirectStatus The redirect status, by default 302 Found.
         *        Use 301 for permanent moves, 302/303/307/308 for temporary ones.
         * @param constraints Array of HTTP methods allowed for this redirect.
         *        Defaults to `{"GET"}` (GET-only). Pass an empty array to allow all methods.
         */
        void redirect(const std::string &initialPath, const std::string &destinationPath, int redirectStatus = 302,
                      const std::vector<std::string> &constraints = {"GET"}) const;

        /** @return Cached entity schema JSON for `table_name`. */
        const json &schemaCache(const std::string &table_name) const;

        [[nodiscard]] bool hasSchemaCache(const std::string &table_name) const;

        /** @return Cached @ref Entity built from the schema cache entry. */
        Entity schemaCacheEntity(const std::string &table_name) const;

        void addSchemaCache(const nlohmann::json &entity_schema) const;

        void updateSchemaCache(const std::string &old_entity_name, const json &new_schema) const;

        void removeSchemaCache(const std::string &entity_name) const;

        /** Like @ref addSchemaCache but assumes the caller already holds the schema mutex. */
        void addSchemaCacheLocked(const nlohmann::json &entity_schema) const;

        /** Like @ref removeSchemaCache but assumes the caller already holds the schema mutex. */
        void removeSchemaCacheLocked(const std::string &entity_name) const;

        [[nodiscard]] bool isRunning() const;

        /** Reload allowed CORS origins from settings and MB_CORS_ORIGINS. */
        void reloadCorsOrigins();

        /** Add CORS headers when the request Origin is in the allowlist. */
        void applyCorsHeaders(const drogon::HttpRequestPtr &req,
                              const drogon::HttpResponsePtr &resp) const;

        const std::vector<MiddlewareFn> &preRoutingMiddlewares() const { return m_preRoutingMiddlewares; }

#ifdef MB_SCRIPTING_ENABLED
        /** Register a custom route from JavaScript (`app.router().addRoute(...)`). */
        duk_ret_t bindRoute(duk_context *ctx);

        /** Shorthand for `addRoute("GET", path, handler, ...middlewares)`. */
        duk_ret_t bindGet(duk_context *ctx);
        /** Shorthand for `addRoute("POST", path, handler, ...middlewares)`. */
        duk_ret_t bindPost(duk_context *ctx);
        /** Shorthand for `addRoute("PATCH", path, handler, ...middlewares)`. */
        duk_ret_t bindPatch(duk_context *ctx);
        /** Shorthand for `addRoute("DELETE", path, handler, ...middlewares)`. */
        duk_ret_t bindDelete(duk_context *ctx);

        /**
         * Register a path redirect from JavaScript
         * (`app.router().redirect(from, to[, status[, methods]])`).
         */
        duk_ret_t bindRedirect(duk_context *ctx);

        /** Push a change event to all realtime subscribers. */
        void broadcastChange(const nlohmann::json &change_event) const;

        void broadcastChangeJson(const std::string &event_json) const;

        static void executeJsRoute(const DukValue &handler,
                            const std::vector<DukValue> &middlewares,
                            MantisRequest &req,
                            MantisResponse &res) ;
#endif

    private:
        void registerDrogonHandler(const std::string &method, const std::string &path) const;

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

        static std::function<void(MantisRequest &, MantisResponse &)> fileServingHandler();

        static std::function<void(const MantisRequest &, MantisResponse &)> healthCheckHandler();

        ///> Sync Advice to return handler that generates unique IDs per request
        const std::function<drogon::HttpResponsePtr(const drogon::HttpRequestPtr &)> reqIdSyncAdvice();

        ///> Returns handler logger func for all requests before they are sent
        std::function<void(const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp)>
        loggerPreSendingAdvice() const;

        bool isOriginAllowed(const std::string &origin) const;

        ///> Register CORS pre-routing advice
        std::function<void(const drogon::HttpRequestPtr &,
                           drogon::AdviceCallback &&,
                           drogon::AdviceChainCallback &&
        )> corsPreRoutingAdvice();

        ///> Register pre-sending advice for CORS headers on all responses
        std::function<void(const drogon::HttpRequestPtr &,
                           const drogon::HttpResponsePtr &resp)> corsPreSendingAdvice() const;

        ///> Get default 404 handler
        static drogon::HttpResponsePtr default404Response();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthVerify();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogin();

        static std::function<void(MantisRequest &, MantisResponse &)> handleAdminLogin();

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthRefresh() const;

        std::function<void(MantisRequest &, MantisResponse &)> handleAuthLogout();

        std::function<void(MantisRequest &, MantisResponse &)> handleSetupAdmin();

        static std::function<void(const MantisRequest &, MantisResponse &)> handleLogs();

        using Constraints = std::vector<drogon::internal::HttpConstraint>;

        /**
         * Validate routing constraints, only allow: Get, Post, Head, Put, Delete, Options, Patch methods
         * @param constraints A vector of HTTP request methods
         * @returns vector of HttpConstraint type
         */
        static Constraints validateConstraints(const std::vector<std::string> &constraints);;

        RouteRegistry m_routeRegistry;
        std::unique_ptr<SSEMgr> m_sseMgr;
        std::vector<MiddlewareFn> m_preRoutingMiddlewares;
        std::vector<HandlerFn> m_postRoutingMiddlewares;

        /// Entity schema cache. Read on every request by the http worker
        /// threads and mutated at runtime by the schema CRUD endpoints, so all
        /// access must be synchronized via m_entityMapMutex.
        mutable std::unordered_map<std::string, Entity> m_entityMap;
        std::atomic<bool> m_running{false};
        mutable std::shared_mutex m_entityMapMutex;

        Snowflake<1534832906275L> m_sfId;

        std::atomic<std::shared_ptr<const std::set<std::string>> > m_corsAllowedOrigins;
    };
} // mb

#endif // MB_ROUTER_H
