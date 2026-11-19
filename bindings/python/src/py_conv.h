#pragma once

#include <mantisbase/core/types.h>
#include <memory>
#include <nanobind/nanobind.h>
#include <nlohmann/json.hpp>
#include <vector>

namespace nb = nanobind;

namespace mbpy {
    nb::object json_to_python(const nlohmann::json &value);

    nlohmann::json python_to_json(nb::handle value);

    /** Dict, or an empty object when @p value is None. */
    nlohmann::json object_or_empty(nb::handle value);

    struct PyMiddleware {
        mb::MbMiddlewareFn fn;
    };

    /**
     * Python object kept alive by the C++ router. Route handlers and Python
     * middleware close over the caller's globals, which own the App, so a strong
     * reference here is a cycle the cyclic GC cannot see unless it is visited.
     */
    struct PyRefPin {
        nb::object obj;
    };

    std::shared_ptr<PyRefPin> retain_py_ref(const void *owner, nb::handle obj);

    nb::object load_pin(const std::shared_ptr<PyRefPin> &pin);

    void copy_retained(const void *owner, std::vector<nb::object> &out);

    void clear_retained(const void *owner);

    /** Accept a middleware, a callable, or a list/tuple of either. */
    mb::MbMiddlewares collect_middlewares(nb::handle middlewares,
                                          const void *owner);

    void register_mantis_error(nb::module_ & m);

    template<typename Fn>
    nb::object call_json(Fn &&fn) {
        nlohmann::json result;
        {
            nb::gil_scoped_release release;
            result = std::forward<Fn>(fn)();
        }
        return json_to_python(result);
    }
} // namespace mbpy
