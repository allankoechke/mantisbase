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

    class IMantisBase {
        /// Non-owning pointer to the owning application, set from the constructor
        /// reference. A raw pointer (rather than a reference) keeps Entity
        /// copy/move-assignable, which the router's entity cache relies on; it is
        /// never null after construction.
        const MantisBase& m_app;

    public:
        explicit IMantisBase(const MantisBase& app);

        [[nodiscard]] const MantisBase &mbApp() const;

        [[nodiscard]] const Logger& logger() const;
    };

#define REQUEST_HANDLED HandlerResponse::Handled;
#define REQUEST_PENDING HandlerResponse::Unhandled;
}

#endif //MANTISBASE_TYPES_H
