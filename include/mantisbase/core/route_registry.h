/**
 * @file route_registry.h
 * @brief In-memory HTTP route table keyed by method and path.
 *
 * Used by @ref MbRouter to register MantisBase handlers and middleware chains before
 * they are mirrored into Drogon's native routing layer.
 */

#ifndef ROUTE_REGISTRY_H
#define ROUTE_REGISTRY_H

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "../utils/utils.h"
#include "types.h"
#include "logger/logger.h"

namespace mb
{
    /** Hash functor for @ref RouteKey (`method`, `path`) pairs. */
    struct MbRouteKeyHash
    {
        size_t operator()(const MbRouteKey& k) const;
    };

    /** Handler plus ordered middleware chain for a single route. */
    struct MANTISBASE_API MbRouteHandler
    {
        std::vector<MbMiddlewareFn> middlewares;
        MbHandlerFn handler;
    };

    /**
     * @brief Thread-local route registry backing the MantisBase router.
     */
    class MANTISBASE_API MbRouteRegistry
    {
        std::unordered_map<MbRouteKey, MbRouteHandler, MbRouteKeyHash> routes;

    public:
        /** Register a handler for `method` + `path`. */
        void add(const std::string& method,
                 const std::string& path,
                 const MbHandlerFn &handler,
                 const MbMiddlewares& middlewares);

        /** @return Route handler metadata, or `nullptr` if not registered. */
        const MbRouteHandler* find(const std::string& method, const std::string& path) const;

        /**
         * @brief Remove a route from the registry.
         * @return JSON status payload describing the removal result.
         */
        json remove(const std::string& method, const std::string& path);

        /**
         * Clear all registered routes
         */
        void clear() {
            routes.clear();
        }
    };
}

#endif // ROUTE_REGISTRY_H
