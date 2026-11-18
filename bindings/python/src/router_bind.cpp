#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <cctype>
#include <mantisbase/core/router.h>

#include "py_conv.h"

namespace nb = nanobind;

using RegisterMethod = void (mb::MbRouter::*)(const std::string &, const mb::MbHandlerFn &, const mb::MbMiddlewares &);

namespace {

void register_route(mb::MbRouter &router, const std::string &path, const nb::callable &handler,
                    RegisterMethod method, mb::MbMiddlewares middlewares) {
    nb::callable stored(handler);
    const mb::MbHandlerFn fn = [stored](mb::MbRequest &req, mb::MbResponse &res) {
        nb::gil_scoped_acquire gil;
        try {
            stored(&req, &res);
        } catch (nb::python_error &error) {
            const std::string message = error.what();
            error.restore();
            PyErr_Clear();
            res.sendJSON(500, nlohmann::json{{"error", message}});
        }
    };
    (router.*method)(path, fn, std::move(middlewares));
}

nb::object make_route_method(mb::MbRouter &router, const std::string &path, nb::object handler,
                             nb::handle middlewares, RegisterMethod method) {
    mb::MbMiddlewares chain = mbpy::collect_middlewares(middlewares);
    if (!handler.is_none()) {
        register_route(router, path, nb::callable(handler, nb::detail::borrow_t{}), method, std::move(chain));
        return handler;
    }

    mb::MbRouter *router_ptr = &router;
    return nb::cpp_function([router_ptr, path, method, chain](nb::callable handler) {
        register_route(*router_ptr, path, handler, method, chain);
        return handler;
    });
}

RegisterMethod method_for(std::string method) {
    for (char &ch : method)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (method == "GET")
        return &mb::MbRouter::Get;
    if (method == "POST")
        return static_cast<RegisterMethod>(&mb::MbRouter::Post);
    if (method == "PUT")
        return static_cast<RegisterMethod>(&mb::MbRouter::Put);
    if (method == "PATCH")
        return static_cast<RegisterMethod>(&mb::MbRouter::Patch);
    if (method == "DELETE")
        return &mb::MbRouter::Delete;
    throw nb::value_error("method must be GET, POST, PUT, PATCH, or DELETE");
}

std::vector<std::string> redirect_methods(nb::handle methods) {
    if (!methods.is_valid() || methods.is_none())
        return {"GET"};
    if (!PyList_Check(methods.ptr()) && !PyTuple_Check(methods.ptr()))
        throw nb::type_error("methods must be a list of HTTP method names");

    std::vector<std::string> out;
    const Py_ssize_t count = PySequence_Size(methods.ptr());
    out.reserve(static_cast<size_t>(count));
    for (Py_ssize_t i = 0; i < count; ++i) {
        nb::object item = nb::steal<nb::object>(PySequence_GetItem(methods.ptr(), i));
        out.push_back(nb::cast<std::string>(item));
    }
    return out;
}

} // namespace

void register_router(nb::module_ &m) {
    auto route = [](RegisterMethod method) {
        return [method](mb::MbRouter &self, const std::string &path, nb::object handler, nb::handle middlewares) {
            return make_route_method(self, path, std::move(handler), middlewares, method);
        };
    };

    nb::class_<mb::MbRouter>(m, "MbRouter", "HTTP route table for one MantisBase app")
        .def("get", route(&mb::MbRouter::Get), nb::arg("path"), nb::arg("handler") = nb::none(),
             nb::arg("middlewares") = nb::none())
        .def("post", route(static_cast<RegisterMethod>(&mb::MbRouter::Post)), nb::arg("path"),
             nb::arg("handler") = nb::none(), nb::arg("middlewares") = nb::none())
        .def("put", route(static_cast<RegisterMethod>(&mb::MbRouter::Put)), nb::arg("path"),
             nb::arg("handler") = nb::none(), nb::arg("middlewares") = nb::none())
        .def("patch", route(static_cast<RegisterMethod>(&mb::MbRouter::Patch)), nb::arg("path"),
             nb::arg("handler") = nb::none(), nb::arg("middlewares") = nb::none())
        .def("delete", route(&mb::MbRouter::Delete), nb::arg("path"), nb::arg("handler") = nb::none(),
             nb::arg("middlewares") = nb::none())
        .def("add_route",
             [](mb::MbRouter &self, const std::string &method, const std::string &path, nb::object handler,
                nb::handle middlewares) {
                 return make_route_method(self, path, std::move(handler), middlewares, method_for(method));
             },
             nb::arg("method"), nb::arg("path"), nb::arg("handler") = nb::none(), nb::arg("middlewares") = nb::none())
        .def("redirect",
             [](mb::MbRouter &self, const std::string &source, const std::string &destination, int status,
                nb::handle methods) {
                 self.redirect(source, destination, status, redirect_methods(methods));
             },
             nb::arg("source"), nb::arg("destination"), nb::arg("status") = 302, nb::arg("methods") = nb::none(),
             "Redirect a path. methods defaults to ['GET']; pass [] to allow every method.")
        .def("broadcast",
             [](mb::MbRouter &self, nb::handle event) {
                 self.sseMgr().broadcastChange(mbpy::python_to_json(event));
             },
             nb::arg("event"), "Push a change event to SSE and WebSocket subscribers.")
        .def_prop_ro("is_running", &mb::MbRouter::isRunning)
        .def_prop_ro("sse", &mb::MbRouter::sseMgr, nb::rv_policy::reference_internal);
}
