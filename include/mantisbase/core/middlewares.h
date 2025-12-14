/**
 * @file middlewares.h
 * @brief Middleware functions for request processing.
 *
 * Provides pre-built middleware functions for authentication, authorization,
 * and request context management.
 */

#ifndef MANTISBASE_MIDDLEWARES_H
#define MANTISBASE_MIDDLEWARES_H

#include <functional>
#include <string>
#include "types.h"
#include "models/entity.h"

namespace mb {
    /**
     * @brief Extract and validate JWT token from Authorization header.
     *
     * Parses Bearer token from request headers and stores user info in context.
     * @return Middleware function
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> getAuthToken();
    
    /**
     * @brief Hydrate request context with additional data.
     *
     * Populates context store with request metadata and user information.
     * @return Middleware function
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> hydrateContextData();
    
    /**
     * @brief Check if request has access to entity based on access rules.
     * @param entity_name Entity/table name to check access for
     * @return Middleware function that validates access rules
     * @code
     * router.Get("/api/v1/posts", handler, {hasAccess("posts")});
     * @endcode
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> hasAccess(const std::string& entity_name);
    
    /**
     * @brief Require expression evaluation to pass.
     * @param expr Expression string to evaluate
     * @return Middleware function that evaluates expression
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireExprEval(const std::string& expr);
    
    /**
     * @brief Require guest-only access (no authentication).
     *
     * Blocks authenticated users, only allows unauthenticated requests.
     * @return Middleware function
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireGuestOnly();
    
    /**
     * @brief Require admin authentication.
     *
     * Only allows requests from users authenticated as admins.
     * @return Middleware function
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireAdminAuth();
    
    /**
     * @brief Require admin OR entity authentication.
     * @param entity_name Entity name for entity-based auth
     * @return Middleware function that allows admins or entity users
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireAdminOrEntityAuth(const std::string& entity_name);
    
    /**
     * @brief Require entity-specific authentication.
     * @param entity_name Entity name to authenticate against
     * @return Middleware function that validates entity auth
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireEntityAuth(const std::string& entity_name);
}

#endif //MANTISBASE_MIDDLEWARES_H