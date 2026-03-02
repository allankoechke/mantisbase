/**
 * @file middlewares.h
 * Middleware functions for request processing.
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
     * Extract and validate JWT token from Authorization header.
     *
     * Parses Bearer token from request headers and stores user info in context.
     * @return Middleware function
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> getAuthToken();
    
    /**
     * Hydrate request context with additional data.
     *
     * Populates context store with request metadata and user information.
     * @return Middleware function
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> hydrateContextData();
    
    /**
     * Check if request has access to entity based on access rules.
     * @param entity_name Entity/table name to check access for
     * @return Middleware function that validates access rules
     * @code
     * router.Get("/api/v1/posts", handler, {hasAccess("posts")});
     * @endcode
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> hasAccess(const std::string& entity_name);
    
    /**
     * Require expression evaluation to pass.
     * @param expr Expression string to evaluate
     * @return Middleware function that evaluates expression
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireExprEval(const std::string& expr);
    
    /**
     * Require guest-only access (no authentication).
     *
     * Blocks authenticated users, only allows unauthenticated requests.
     * @return Middleware function
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireGuestOnly();
    
    /**
     * Require admin authentication.
     *
     * Only allows requests from users authenticated as admins.
     * @return Middleware function
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireAdminAuth();
    
    /**
     * Require admin OR entity authentication.
     * @param entity_name Entity name for entity-based auth
     * @return Middleware function that allows admins or entity users
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireAdminOrEntityAuth(const std::string& entity_name);
    
    /**
     * Require entity-specific authentication.
     * @param entity_name Entity name to authenticate against
     * @return Middleware function that validates entity auth
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireEntityAuth(const std::string& entity_name);
    
    /**
     * Rate limiting middleware to prevent abuse.
     * 
     * Limits the number of requests per time window. Can rate limit by IP address
     * or by authenticated user ID. When the rate limit is exceeded, returns a
     * 429 Too Many Requests response with rate limit headers.
     * 
     * @param max_requests Maximum number of requests allowed in the time window
     * @param window_seconds Time window duration in seconds
     * @param use_user_id If true, rate limit by authenticated user ID; if false, use IP address.
     *                    Falls back to IP if user ID is not available.
     * @return Middleware function that enforces rate limits
     * 
     * @note The login endpoint uses this middleware with 5 requests per 60 seconds by IP.
     * @note Rate limit headers are included in responses: X-RateLimit-Limit, X-RateLimit-Remaining,
     *       X-RateLimit-Reset, and Retry-After.
     * 
     * @code
     * // Rate limit by IP: 100 requests per minute
     * router.Get("/api/v1/data", handler, {rateLimit(100, 60, false)});
     * 
     * // Rate limit by user: 10 requests per second
     * router.Post("/api/v1/upload", handler, {rateLimit(10, 1, true)});
     * 
     * // Login endpoint: 5 attempts per minute per IP (prevents brute force)
     * router.Post("/api/v1/auth/login", loginHandler, {rateLimit(5, 60, false)});
     * @endcode
     * @ingroup cpp_middlewares
     */
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> rateLimit(
        int max_requests, 
        int window_seconds, 
        bool use_user_id = false
    );
}

#endif //MANTISBASE_MIDDLEWARES_H