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

#define REQUEST_HANDLED HandlerResponse::Handled;
#define REQUEST_PENDING HandlerResponse::Unhandled;
}

#endif //MANTISBASE_TYPES_H
