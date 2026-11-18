#pragma once

#include <nanobind/nanobind.h>
#include <nlohmann/json.hpp>
#include <mantisbase/core/types.h>

namespace nb = nanobind;

namespace mbpy {

nb::object json_to_python(const nlohmann::json &value);
nlohmann::json python_to_json(nb::handle value);

/** Dict, or an empty object when @p value is None. */
nlohmann::json object_or_empty(nb::handle value);

struct PyMiddleware {
    mb::MbMiddlewareFn fn;
};

/** Accept a middleware, a callable, or a list/tuple of either. */
mb::MbMiddlewares collect_middlewares(nb::handle middlewares);

void register_mantis_error(nb::module_ &m);

template <typename Fn>
nb::object call_json(Fn &&fn) {
    nlohmann::json result;
    {
        nb::gil_scoped_release release;
        result = std::forward<Fn>(fn)();
    }
    return json_to_python(result);
}

} // namespace mbpy
