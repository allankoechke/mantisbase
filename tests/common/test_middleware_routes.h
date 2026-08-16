#ifndef MANTISBASE_TEST_MIDDLEWARE_ROUTES_H
#define MANTISBASE_TEST_MIDDLEWARE_ROUTES_H

#include <format>

#include "mantisbase/core/middlewares.h"
#include "mantisbase/core/router.h"

namespace TestFixture {

inline void registerMiddlewareTestRoutes(mb::Router &router) {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    static constexpr const char *kEntity = "test_users";

    const mb::HandlerFn okHandler = [](const mb::MantisRequest &, const mb::MantisResponse &res) {
        res.sendJSON(200, {
                             {"status", 200},
                             {"data", {{"ok", true}}},
                             {"error", ""}
                         });
    };

    router.Get("/api/v1/test/middleware/expr",
               okHandler,
               {mb::requireExprEval(std::format(R"(@auth.entity == '{}')", kEntity))});
    router.Get("/api/v1/test/middleware/entity-auth",
               okHandler,
               {mb::requireEntityAuth(kEntity)});
    router.Get("/api/v1/test/middleware/admin-or-entity",
               okHandler,
               {mb::requireAdminOrEntityAuth(kEntity)});
}

} // namespace TestFixture

#endif // MANTISBASE_TEST_MIDDLEWARE_ROUTES_H
