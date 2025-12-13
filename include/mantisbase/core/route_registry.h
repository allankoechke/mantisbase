/**
 * @file route_registry.h
 * @brief Route registry
 *
 * Created by allan on 12/05/2025.
 */

#ifndef ROUTE_REGISTRY_H
#define ROUTE_REGISTRY_H

#include <httplib.h>
#include <unordered_map>
#include <functional>
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

#include "../utils/utils.h"
#include  "types.h"
#include "logger.h"

namespace mb
{
    /**
     * Structure to allow for hashing of the `RouteKey` for use in std::unordered_map as a key.
     */
    struct RouteKeyHash
    {
        /**
         * @brief Operator function called when hashing RouteKey is required.
         * @param k RouteKey pair
         * @return Hash of RouteKey
         */
        size_t operator()(const RouteKey& k) const;
    };

    /**
     * @brief Struct encompassing the list of middlewares and the handler function registered to a specific route.
     */
    struct RouteHandler
    {
        std::vector<MiddlewareFn> middlewares; ///> List of @see Middlewares for a route.
        std::variant<HandlerFn, HandlerWithContentReaderFn> handler; ///> Handler function for a route
    };

    /**
     * Class to manage route registration, removal and dynamic checks on request.
     */
    class RouteRegistry
    {
        /// Map holding the route key to route handler mappings.
        std::unordered_map<RouteKey, RouteHandler, RouteKeyHash> routes;

    public:
        /**
         * @brief Add new route to the registry.
         *
         * @param method Request method, i.e. GET, POST, PATCH, etc.
         * @param path Request path.
         * @param handler Request handler function.
         * @param middlewares List of @see Middleware to be imposed on this request         *
         */
        void add(const std::string& method,
                 const std::string& path,
                 HandlerFn handler,
                 const Middlewares& middlewares);
        /**
         * @brief Add new route to the registry.
         *
         * @param method Request method, i.e. GET, POST, PATCH, etc.
         * @param path Request path.
         * @param handler Request handler function.
         * @param middlewares List of @see Middleware to be imposed on this request         *
         */
        void add(const std::string& method,
                 const std::string& path,
                 HandlerWithContentReaderFn handler,
                 const Middlewares& middlewares);
        /**
         * @brief Find a route in the registry matching given method and route.
         *
         * @param method Request method.
         * @param path Request path.
         * @return @see RouteHandler struct having middlewares and handler func.
         */
        const RouteHandler* find(const std::string& method, const std::string& path) const;

        /**
         * @brief Remove find and remove existing route + path pair from the registry
         *
         * @param method Request method
         * @param path Request path
         * @return JSON Error object, error value contains data if operation fails.
         */
        json remove(const std::string& method, const std::string& path);

        const std::string __class_name__ = "mb::RouteRegistry";
    };
}

#endif // ROUTE_REGISTRY_H
