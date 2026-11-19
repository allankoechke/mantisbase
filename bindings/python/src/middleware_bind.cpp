#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/core/http.h>
#include <mantisbase/core/middlewares.h>

#include "py_conv.h"

namespace nb = nanobind;

void register_middleware(nb::module_ &m) {
    nb::class_<mbpy::PyMiddleware>(m, "Middleware",
                                   "C++ middleware. Use it in a route's middlewares argument, or call it as (req, res) -> bool.")
        .def("__call__", [](const mbpy::PyMiddleware &self, mb::MbRequest &req, mb::MbResponse &res) {
            return self.fn(req, res) == mb::MbHandlerResponse::Unhandled;
        }, nb::arg("req"), nb::arg("res"));

    nb::module_ mw = m.def_submodule("middlewares", "Built-in request middleware factories");

    auto wrap = [](mb::MbMiddlewareFn fn) { return mbpy::PyMiddleware{std::move(fn)}; };

    mw.def("get_auth_token", [=]() { return wrap(mb::getAuthToken()); },
           "Read a Bearer token or mb_token cookie and store auth on the request.");
    mw.def("hydrate_context", [=]() { return wrap(mb::hydrateContextData()); });
    mw.def("resolve_schema", [=]() { return wrap(mb::resolveSchema()); });
    mw.def("resolve_auth_entity", [=]() { return wrap(mb::resolveAuthEntity()); });
    mw.def("resolve_entity", [=]() { return wrap(mb::resolveEntity()); });
    mw.def("reject_view_mutations", [=]() { return wrap(mb::rejectViewMutations()); });
    mw.def("has_entity_access", [=]() { return wrap(mb::hasEntityAccess()); });
    mw.def("require_guest", [=]() { return wrap(mb::requireGuestOnly()); });
    mw.def("require_admin", [=]() { return wrap(mb::requireAdminAuth()); });
    mw.def("has_access", [=](const std::string &entity) { return wrap(mb::hasAccess(entity)); }, nb::arg("entity"));
    mw.def("require_expr", [=](const std::string &expr) { return wrap(mb::requireExprEval(expr)); }, nb::arg("expr"));
    mw.def("settings_gate", [=](const std::string &key) { return wrap(mb::settingsFeatureGate(key)); }, nb::arg("key"));
    mw.def("require_entity", [=](const std::string &entity) { return wrap(mb::requireEntityAuth(entity)); },
           nb::arg("entity"));
    mw.def("require_admin_or_entity",
           [=](const std::string &entity) { return wrap(mb::requireAdminOrEntityAuth(entity)); }, nb::arg("entity"));
    mw.def("env_gate",
           [=](const std::string &name, bool block_when_truthy) {
               return wrap(mb::envGateMiddleware(name, block_when_truthy));
           },
           nb::arg("name"), nb::arg("block_when_truthy") = true,
           "Respond 503 when an environment variable's truth matches block_when_truthy.");
    mw.def("rate_limit",
           [=](int max_requests, int window_seconds, bool by_user) {
               return wrap(mb::rateLimit(max_requests, window_seconds, by_user));
           },
           nb::arg("max_requests"), nb::arg("window_seconds"), nb::arg("by_user") = false);
}
