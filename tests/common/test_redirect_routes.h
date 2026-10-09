#ifndef MANTISBASE_TEST_REDIRECT_ROUTES_H
#define MANTISBASE_TEST_REDIRECT_ROUTES_H

#include "mantisbase/core/router.h"

namespace TestFixture {

/// C++ redirect routes for Router::redirect integration tests.
/// Registered once before the shared test server starts listening
/// (Router::redirect calls drogon::app().registerHandler directly,
/// so registration must happen pre-listen like all other routes).
inline void registerRedirectTestRoutes(mb::Router &router) {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    const mb::MbHandlerFn targetHandler = [](const mb::MbRequest &, const mb::MbResponse &res) {
        res.sendJSON(200, {
                             {"status", 200},
                             {"data", {{"redirect_target", true}}},
                             {"error", ""}
                         });
    };

    router.Get("/api/v1/test/redirect/target", targetHandler);

    // Default: 302 Found, GET-only (mirrors Router::redirect defaults).
    router.redirect("/api/v1/test/redirect/old", "/api/v1/test/redirect/target");

    // Permanent move.
    router.redirect("/api/v1/test/redirect/old-301", "/api/v1/test/redirect/target", 301);

    // Multi-method redirect (GET + POST).
    router.redirect("/api/v1/test/redirect/old-multi",
                    "/api/v1/test/redirect/target",
                    307,
                    {"GET", "POST"});

    // Empty constraints allow every method.
    router.redirect("/api/v1/test/redirect/allow-all",
                    "/api/v1/test/redirect/target",
                    302,
                    {});
}

} // namespace TestFixture

#endif // MANTISBASE_TEST_REDIRECT_ROUTES_H
