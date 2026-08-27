/**
 * @file route_registry.h
 * @brief In-memory HTTP route table keyed by method and path.
 *
 * Used by @ref Router to register MantisBase handlers and middleware chains before
 * they are mirrored into Drogon's native routing layer.
 */

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
    /** Hash functor for @ref RouteKey (`method`, `path`) pairs. */
    struct RouteKeyHash
    {
        size_t operator()(const RouteKey& k) const;
    };

    /** Handler plus ordered middleware chain for a single route. */
    struct RouteHandler
    {
        std::vector<MiddlewareFn> middlewares;
        std::variant<HandlerFn, HandlerWithContentReaderFn> handler;
    };

    /**
     * @brief Thread-local route registry backing the MantisBase router.
     *
     * Supports both plain and content-reader handler variants (multipart uploads).
     */
    class RouteRegistry
    {
        std::unordered_map<RouteKey, RouteHandler, RouteKeyHash> routes;

    public:
        /** Register a standard handler for `method` + `path`. */
        void add(const std::string& method,
                 const std::string& path,
                 const HandlerFn &handler,
                 const Middlewares& middlewares);

        /** Register a handler that receives a @ref MantisContentReader. */
        void add(const std::string& method,
                 const std::string& path,
                 const HandlerWithContentReaderFn &handler,
                 const Middlewares& middlewares);

        /** @return Route handler metadata, or `nullptr` if not registered. */
        const RouteHandler* find(const std::string& method, const std::string& path) const;

        /**
         * @brief Remove a route from the registry.
         * @return JSON status payload describing the removal result.
         */
        json remove(const std::string& method, const std::string& path);
    };
}

#endif // ROUTE_REGISTRY_H
