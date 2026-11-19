#include <cctype>
#include <mantisbase/core/router.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "py_conv.h"

namespace nb = nanobind;

using RegisterMethod = void (mb::MbRouter::*)(const std::string &,
                                              const mb::MbHandlerFn &,
                                              const mb::MbMiddlewares &);

namespace {
    bool same_object(nb::handle left, nb::handle right) {
        const int compared = PyObject_RichCompareBool(left.ptr(), right.ptr(), Py_EQ);
        if (compared < 0)
            throw nb::python_error();
        return compared == 1;
    }

    // How many of (req, res) this callable can accept. *args receives both.
    // A function with no parameters is called with none, so `def root(): return
    // {...}` works.
    int handler_pass_count(const nb::callable &handler) {
        try {
            nb::object inspect = nb::module_::import_("inspect");
            nb::object parameter = inspect.attr("Parameter");
            nb::handle positional_only = parameter.attr("POSITIONAL_ONLY");
            nb::handle positional_or_keyword = parameter.attr("POSITIONAL_OR_KEYWORD");
            nb::handle var_positional = parameter.attr("VAR_POSITIONAL");
            nb::handle empty = parameter.attr("empty");

            nb::object signature = inspect.attr("signature")(handler);
            int positional = 0;
            int required = 0;
            bool varargs = false;
            for (nb::handle param:
                 nb::iter(signature.attr("parameters").attr("values")())) {
                nb::handle kind = param.attr("kind");
                if (same_object(kind, var_positional)) {
                    varargs = true;
                    continue;
                }
                if (!same_object(kind, positional_only) &&
                    !same_object(kind, positional_or_keyword))
                    continue;
                ++positional;
                if (same_object(param.attr("default"), empty))
                    ++required;
            }
            if (!varargs && required > 2)
                throw nb::type_error(
                    "route handler accepts at most request and response");
            if (varargs || positional >= 2)
                return 2;
            return positional;
        } catch (nb::python_error &) {
            PyErr_Clear();
            return 2;
        }
    }

    void register_route(mb::MbRouter &router, const std::string &path,
                        const nb::callable &handler, RegisterMethod method,
                        mb::MbMiddlewares middlewares) {
        const int pass_count = handler_pass_count(handler);
        const std::shared_ptr<mbpy::PyRefPin> pin =
                mbpy::retain_py_ref(&router, handler);
        const mb::MbHandlerFn fn = [pin, pass_count](mb::MbRequest &req,
                                                     mb::MbResponse &res) {
            nb::gil_scoped_acquire gil;
            const nb::object callback = mbpy::load_pin(pin);
            if (!callback.is_valid()) {
                res.sendJSON(500, nlohmann::json{
                                 {"error", "route handler is no longer available"}
                             });
                return;
            }
            try {
                nb::callable stored = nb::borrow<nb::callable>(callback);
                nb::object result = nb::none();
                if (pass_count >= 2)
                    result = stored(&req, &res);
                else if (pass_count == 1)
                    result = stored(&req);
                else
                    result = stored();
                if (result.is_valid() && !result.is_none())
                    res.sendJSON(200, mbpy::python_to_json(result));
            } catch (nb::python_error &error) {
                const std::string message = error.what();
                error.restore();
                PyErr_Clear();
                res.sendJSON(500, nlohmann::json{{"error", message}});
            }
        };
        (router.*method)(path, fn, std::move(middlewares));
    }

    nb::object make_route_method(mb::MbRouter &router, const std::string &path,
                                 nb::object handler, nb::handle middlewares,
                                 RegisterMethod method) {
        mb::MbMiddlewares chain = mbpy::collect_middlewares(middlewares, &router);
        if (!handler.is_none()) {
            register_route(router, path, nb::callable(handler, nb::detail::borrow_t{}),
                           method, std::move(chain));
            return handler;
        }

        mb::MbRouter *router_ptr = &router;
        return nb::cpp_function(
            [router_ptr, path, method, chain](nb::callable handler) {
                register_route(*router_ptr, path, handler, method, chain);
                return handler;
            });
    }

    RegisterMethod method_for(std::string method) {
        for (char &ch: method)
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
            nb::object item =
                    nb::steal<nb::object>(PySequence_GetItem(methods.ptr(), i));
            out.push_back(nb::cast<std::string>(item));
        }
        return out;
    }
} // namespace

void register_router(nb::module_ &m) {
    auto route = [](RegisterMethod method) {
        return [method](mb::MbRouter &self, const std::string &path,
                        nb::object handler, nb::handle middlewares) {
            return make_route_method(self, path, std::move(handler), middlewares,
                                     method);
        };
    };

    nb::class_<mb::MbRouter>(m, "MbRouter",
                             "HTTP route table for one MantisBase app")
            .def("get", route(&mb::MbRouter::Get), nb::arg("path"),
                 nb::arg("handler") = nb::none(), nb::arg("middlewares") = nb::none())
            .def("post", route(static_cast<RegisterMethod>(&mb::MbRouter::Post)),
                 nb::arg("path"), nb::arg("handler") = nb::none(),
                 nb::arg("middlewares") = nb::none())
            .def("put", route(static_cast<RegisterMethod>(&mb::MbRouter::Put)),
                 nb::arg("path"), nb::arg("handler") = nb::none(),
                 nb::arg("middlewares") = nb::none())
            .def("patch", route(static_cast<RegisterMethod>(&mb::MbRouter::Patch)),
                 nb::arg("path"), nb::arg("handler") = nb::none(),
                 nb::arg("middlewares") = nb::none())
            .def("delete", route(&mb::MbRouter::Delete), nb::arg("path"),
                 nb::arg("handler") = nb::none(), nb::arg("middlewares") = nb::none())
            .def(
                "add_route",
                [](mb::MbRouter &self, const std::string &method,
                   const std::string &path, nb::object handler,
                   nb::handle middlewares) {
                    return make_route_method(self, path, std::move(handler),
                                             middlewares, method_for(method));
                },
                nb::arg("method"), nb::arg("path"), nb::arg("handler") = nb::none(),
                nb::arg("middlewares") = nb::none())
            .def(
                "redirect",
                [](mb::MbRouter &self, const std::string &source,
                   const std::string &destination, int status, nb::handle methods) {
                    self.redirect(source, destination, status,
                                  redirect_methods(methods));
                },
                nb::arg("source"), nb::arg("destination"), nb::arg("status") = 302,
                nb::arg("methods") = nb::none(),
                "Redirect a path. methods defaults to ['GET']; pass [] to allow "
                "every method.")
            .def(
                "broadcast",
                [](mb::MbRouter &self, nb::handle event) {
                    self.sseMgr().broadcastChange(mbpy::python_to_json(event));
                },
                nb::arg("event"),
                "Push a change event to SSE and WebSocket subscribers.")
            .def_prop_ro("is_running", &mb::MbRouter::isRunning)
            .def_prop_ro("sse", &mb::MbRouter::sseMgr,
                         nb::rv_policy::reference_internal);
}
