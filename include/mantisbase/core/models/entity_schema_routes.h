/**
 * @file entity_schema_routes.h
 * @brief Unified schema CRUD route handlers.
 *
 * Factory functions return handlers wired to `/api/v1/schemas` admin routes.
 */

#ifndef MANTISBASE_ENTITY_SCHEMA_ROUTES_H
#define MANTISBASE_ENTITY_SCHEMA_ROUTES_H

#include "../types.h"

namespace mb {
    /** `GET /api/v1/schemas` — list entity schemas. */
    MbHandlerFn schemaGetManyHandler();

    /** `GET /api/v1/schemas/:id` — fetch one schema by id or name. */
    MbHandlerFn schemaGetOneHandler();

    /** `POST /api/v1/schemas` — create a new entity schema and table. */
    MbHandlerFn schemaPostHandler();

    /** `PATCH /api/v1/schemas/:id` — update schema metadata/fields. */
    MbHandlerFn schemaPatchHandler();

    /** `DELETE /api/v1/schemas/:id` — drop schema and underlying table. */
    MbHandlerFn schemaDeleteHandler();
}

#endif // MANTISBASE_ENTITY_SCHEMA_ROUTES_H
