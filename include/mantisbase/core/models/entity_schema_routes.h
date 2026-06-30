/**
 * @file entity_schema_routes.h
 * @brief Unified schema CRUD route handlers.
 */

#ifndef MANTISBASE_ENTITY_SCHEMA_ROUTES_H
#define MANTISBASE_ENTITY_SCHEMA_ROUTES_H

#include "../types.h"

namespace mb {
    HandlerFn schemaGetManyHandler();
    HandlerFn schemaGetOneHandler();
    HandlerFn schemaPostHandler();
    HandlerFn schemaPatchHandler();
    HandlerFn schemaDeleteHandler();
}

#endif // MANTISBASE_ENTITY_SCHEMA_ROUTES_H
