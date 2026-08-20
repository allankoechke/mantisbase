#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/core/router.h>
#include <mantisbase/core/http.h>
#include <mantisbase/core/types.h>

namespace nb = nanobind;

using RegisterMethod = void(mb::Router::*)(const std::string&, const mb::HandlerFn&, const mb::Middlewares&);

static void register_route(mb::Router& router, const std::string& path,
                           nb::callable handler, RegisterMethod method) {
    nb::callable stored_handler(handler);

    mb::HandlerFn fn = [stored_handler](mb::MantisRequest& req, mb::MantisResponse& res) {
        nb::gil_scoped_acquire gil;
        try {
            stored_handler(&req, &res);
        } catch (nb::python_error& e) {
            e.restore();
        }
    };

    (router.*method)(path, fn, {});
}

static nb::object make_route_method(mb::Router& router, const std::string& path,
                                    nb::object handler_or_none, RegisterMethod method) {
    if (!handler_or_none.is_none()) {
        register_route(router, path, nb::callable(handler_or_none), method);
        return handler_or_none;
    }

    mb::Router* router_ptr = &router;
    return nb::cpp_function([router_ptr, path, method](nb::callable handler) -> nb::callable {
        register_route(*router_ptr, path, handler, method);
        return handler;
    });
}

void register_router(nb::module_& m) {
    nb::class_<mb::Router>(m, "Router")
        .def("get", [](mb::Router& self, const std::string& path, nb::object handler) {
            return make_route_method(self, path, handler, &mb::Router::Get);
        }, nb::arg("path"), nb::arg("handler") = nb::none())
        .def("post", [](mb::Router& self, const std::string& path, nb::object handler) {
            return make_route_method(self, path, handler,
                static_cast<RegisterMethod>(&mb::Router::Post));
        }, nb::arg("path"), nb::arg("handler") = nb::none())
        .def("patch", [](mb::Router& self, const std::string& path, nb::object handler) {
            return make_route_method(self, path, handler,
                static_cast<RegisterMethod>(&mb::Router::Patch));
        }, nb::arg("path"), nb::arg("handler") = nb::none())
        .def("delete", [](mb::Router& self, const std::string& path, nb::object handler) {
            return make_route_method(self, path, handler, &mb::Router::Delete);
        }, nb::arg("path"), nb::arg("handler") = nb::none());
}
