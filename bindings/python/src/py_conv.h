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

        ~PyRefPin() {
            // Process exit destroys this after Py_Finalize. nb::object would
            // Py_DECREF into a runtime that is already gone.
            if (!Py_IsInitialized())
                obj.release();
        }
    };

    std::shared_ptr<PyRefPin> retain_py_ref(const void *owner, nb::handle obj);

    nb::object load_pin(const std::shared_ptr<PyRefPin> &pin);

    /**
     * Hand the GC the pinned objects. Must not incref or allocate Python
     * objects: a temporary nb::object created here is destroyed before
     * traverse returns, and that decref crashes CPython 3.10 and 3.13+.
     */
    int visit_retained(const void *owner, visitproc visit, void *arg);

    void clear_retained(const void *owner);

    void clear_all_retained();

    /** Set from the atexit hook, before CPython clears module globals. */
    void note_python_shutdown() noexcept;

    bool python_shutdown_started() noexcept;

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
