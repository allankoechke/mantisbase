#ifndef ROUTE_REGISTRY_H
#define ROUTE_REGISTRY_H

#include <unordered_map>
#include <functional>
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

#include "../utils/utils.h"
#include "types.h"
#include "logger/logger.h"

namespace mb
{
    struct RouteKeyHash
    {
        size_t operator()(const RouteKey& k) const;
    };

    struct RouteHandler
    {
        std::vector<MiddlewareFn> middlewares;
        std::variant<HandlerFn, HandlerWithContentReaderFn> handler;
    };

    class RouteRegistry
    {
        std::unordered_map<RouteKey, RouteHandler, RouteKeyHash> routes;

    public:
        void add(const std::string& method,
                 const std::string& path,
                 const HandlerFn &handler,
                 const Middlewares& middlewares);

        void add(const std::string& method,
                 const std::string& path,
                 const HandlerWithContentReaderFn &handler,
                 const Middlewares& middlewares);

        const RouteHandler* find(const std::string& method, const std::string& path) const;

        json remove(const std::string& method, const std::string& path);

        const std::string __class_name__ = "mb::RouteRegistry";
    };
}

#endif // ROUTE_REGISTRY_H
