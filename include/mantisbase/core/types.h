//
// Created by codeart on 17/11/2025.
//

#ifndef MANTISBASE_TYPES_H
#define MANTISBASE_TYPES_H

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace mantis {
    class MantisBase;
    class MantisRequest;
    class MantisResponse;
    class MantisContentReader;
    class Entity;
    class EntitySchema;
    class EntitySchemaField;

    namespace fs = std::filesystem;

    class KVStore;
    class Database;
    class LogsMgr;
    // class SettingsMgr;
    class Router;
    class Files;

    using json = nlohmann::json;
    using HandlerResponse = httplib::Server::HandlerResponse;

    ///> Route Handler function shorthand
    using HandlerFn = std::function<void(MantisRequest&, MantisResponse&)>;

    ///> Route Handler function with content reader shorthand
    using HandlerWithContentReaderFn = std::function<void(MantisRequest&, MantisResponse&,
                                                                 const MantisContentReader&)>;

    ///> Middleware shorthand for the function
    using MiddlewareFn = std::function<HandlerResponse(MantisRequest&, MantisResponse&)>;

    ///> Middleware function arrays
    using Middlewares = std::vector<MiddlewareFn>;

    ///> Syntactic sugar for request method which is a std::string
    using Method = std::string;

    ///> Syntactic sugar for request path which is a std::string
    using Path = std::string;

    ///> Shorthand notation for the request's method, path pair.
    using RouteKey = std::pair<Method, Path>;

#define REQUEST_HANDLED HandlerResponse::Handled;
#define REQUEST_PENDING HandlerResponse::Unhandled;
}

#endif //MANTISBASE_TYPES_H