/**
 * @file types.h
 * @brief Core type aliases, handler signatures, and the @ref IMantisBase DI mixin.
 *
 * Defines the function types used throughout routing (@ref HandlerFn, @ref MiddlewareFn)
 * and the non-owning @ref MantisBase reference mixin inherited by request/response wrappers
 * and service classes.
 */

#ifndef MANTISBASE_TYPES_H
#define MANTISBASE_TYPES_H

#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>
#include <filesystem>

namespace mb {
    class MantisBase;
    class MantisRequest;
    class MantisResponse;
    class MantisContentReader;
    class Entity;
    class EntitySchema;
    class EntitySchemaField;

    namespace fs = std::filesystem;

    class KeyValStore;
    class Database;
    class Logger;
    class Router;
    class FilesMgr;
    class Auth;
    class ApiKeyManager;
    class OAuthManager;

    using json = nlohmann::json;

    /** Middleware short-circuit result: stop or continue the chain. */
    enum class HandlerResponse {
        Handled,   /**< Response already sent; skip remaining middleware/handler. */
        Unhandled  /**< Continue to the next middleware or route handler. */
    };

    /** Standard route handler `(request, response)`. */
    using HandlerFn = std::function<void(MantisRequest&, MantisResponse&)>;

    /** Route handler with multipart/content reader `(request, response, reader)`. */
    using HandlerWithContentReaderFn = std::function<void(MantisRequest&, MantisResponse&,
                                                                 MantisContentReader&)>;
    using MiddlewareFn = std::function<HandlerResponse(MantisRequest&, MantisResponse&)>;
    using Middlewares = std::vector<MiddlewareFn>;
    using Method = std::string;
    using Path = std::string;

    /** Route lookup key: HTTP method + path pattern. */
    using RouteKey = std::pair<Method, Path>;

    /**
     * @brief Non-owning access to the active @ref MantisBase from DI-aware types.
     *
     * `Entity`, `Router`, `MantisRequest`, `ApiKeyManager`, `OAuthManager`, and
     * other framework types inherit this mixin so handlers and services can call
     * `mbApp()` instead of a removed global singleton.
     *
     * @code
     * router.Get("/api/v1/stats", [](MantisRequest& req, MantisResponse& res) {
     *     auto count = req.mbApp().entity("posts").countRecords();
     *     res.sendJSON(200, {{"posts", count}});
     * });
     * @endcode
     */
    class IMantisBase {
        const MantisBase& m_app;

    public:
        explicit IMantisBase(const MantisBase& app);

        /** @brief Application that owns this service or request context. */
        [[nodiscard]] const MantisBase &mbApp() const;

        /** @return Application logger via @ref MantisBase::logger. */
        [[nodiscard]] const Logger& logger() const;
    };

#define REQUEST_HANDLED HandlerResponse::Handled;
#define REQUEST_PENDING HandlerResponse::Unhandled;
}

#endif //MANTISBASE_TYPES_H
