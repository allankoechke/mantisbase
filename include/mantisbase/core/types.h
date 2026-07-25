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

    enum class HandlerResponse {
        Handled,
        Unhandled
    };

    using HandlerFn = std::function<void(MantisRequest&, MantisResponse&)>;
    using HandlerWithContentReaderFn = std::function<void(MantisRequest&, MantisResponse&,
                                                                 MantisContentReader&)>;
    using MiddlewareFn = std::function<HandlerResponse(MantisRequest&, MantisResponse&)>;
    using Middlewares = std::vector<MiddlewareFn>;
    using Method = std::string;
    using Path = std::string;
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

        [[nodiscard]] const Logger& logger() const;
    };

#define REQUEST_HANDLED HandlerResponse::Handled;
#define REQUEST_PENDING HandlerResponse::Unhandled;
}

#endif //MANTISBASE_TYPES_H
