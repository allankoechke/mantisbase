/**
 * @file entity_routes.h
 * @brief Unified entity CRUD route handlers and registration.
 *
 * Factory functions return handlers wired to `/api/v1/entities/:entity_name` routes.
 * Admin-only entity routes are registered separately via @ref registerAdminEntityRoutes.
 */

#ifndef MANTISBASE_ENTITY_ROUTES_H
#define MANTISBASE_ENTITY_ROUTES_H

#include "../types.h"

namespace mb {
    /** `GET /api/v1/entities/:entity_name` — cursor-paginated list. */
    HandlerFn entityGetManyHandler();

    /** `GET /api/v1/entities/:entity_name/:id` — read one record. */
    HandlerFn entityGetOneHandler();

    /** `POST /api/v1/entities/:entity_name` — create (supports multipart/file fields). */
    HandlerWithContentReaderFn entityPostHandler();

    /** `PATCH /api/v1/entities/:entity_name/:id` — partial update (supports multipart). */
    HandlerWithContentReaderFn entityPatchHandler();

    /** `DELETE /api/v1/entities/:entity_name/:id` — delete one record. */
    HandlerFn entityDeleteHandler();

    /** Register built-in admin entity routes (`mb_admins`, etc.) on the app router. */
    void registerAdminEntityRoutes(const MantisBase& app);
}

#endif // MANTISBASE_ENTITY_ROUTES_H
