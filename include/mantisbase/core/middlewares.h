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
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> getAuthToken();
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> hydrateContextData();
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> hasAccess(const std::string& entity_name);
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireExprEval(const std::string& expr);
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireGuestOnly();
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireAdminAuth();
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireAdminOrEntityAuth(const std::string& entity_name);
    std::function<HandlerResponse(MantisRequest&, MantisResponse&)> requireEntityAuth(const std::string& entity_name);
    // Redirect middleware ?
}

#endif //MANTISBASE_MIDDLEWARES_H