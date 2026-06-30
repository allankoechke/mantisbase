/**
 * @file entity_routes.h
 * @brief Unified entity CRUD route handlers and registration.
 */

#ifndef MANTISBASE_ENTITY_ROUTES_H
#define MANTISBASE_ENTITY_ROUTES_H

#include "../types.h"

namespace mb {
    HandlerFn entityGetManyHandler();
    HandlerFn entityGetOneHandler();
    HandlerWithContentReaderFn entityPostHandler();
    HandlerWithContentReaderFn entityPatchHandler();
    HandlerFn entityDeleteHandler();

    void registerAdminEntityRoutes();
}

#endif // MANTISBASE_ENTITY_ROUTES_H
