#include "py_conv.h"

#include <atomic>
#include <limits>
#include <mantisbase/core/http.h>
#include <mutex>
#include <unordered_map>

namespace mbpy {
    namespace {
        PyObject *g_mantis_error = nullptr;

        nb::object item_at(nb::handle sequence, Py_ssize_t index) {
            return nb::steal<nb::object>(PySequence_GetItem(sequence.ptr(), index));
        }

        std::mutex g_pin_mutex;
        std::atomic<bool> g_py_shutdown{false};
        std::unordered_map<const void *, std::vector<std::shared_ptr<PyRefPin> > > g_pins;

        // nb::cast<std::string> throws std::bad_cast unless this TU sees
        // nanobind's string caster. Go through the Python C API so a JSON
        // string cannot fail that way.
        nb::object unicode_from_utf8(const std::string &text) {
            PyObject *obj = PyUnicode_FromStringAndSize(
                text.data(), static_cast<Py_ssize_t>(text.size()));
            if (!obj) {
                PyErr_Clear();
                throw std::runtime_error("JSON string is not valid UTF-8");
            }
            return nb::steal<nb::object>(obj);
        }

        std::string unicode_to_utf8(PyObject *ptr) {
            Py_ssize_t size = 0;
            const char *data = PyUnicode_AsUTF8AndSize(ptr, &size);
            if (!data) {
                PyErr_Clear();
                throw std::runtime_error("Expected a UTF-8 string");
            }
            return std::string(data, static_cast<std::size_t>(size));
        }
    } // namespace

    std::shared_ptr<PyRefPin> retain_py_ref(const void *owner, nb::handle obj) {
        auto pin = std::make_shared<PyRefPin>();
        pin->obj = nb::borrow(obj);
        std::lock_guard lock(g_pin_mutex);
        g_pins[owner].push_back(pin);
        return pin;
    }

    nb::object load_pin(const std::shared_ptr<PyRefPin> &pin) {
        std::lock_guard lock(g_pin_mutex);
        return pin && pin->obj.is_valid() ? pin->obj : nb::object();
    }

    int visit_retained(const void *owner, visitproc visit, void *arg) {
        std::vector<PyObject *> held;
        {
            std::lock_guard lock(g_pin_mutex);
            const auto found = g_pins.find(owner);
            if (found == g_pins.end())
                return 0;
            held.reserve(found->second.size());
            for (const auto &pin: found->second) {
                if (pin && pin->obj.is_valid())
                    held.push_back(pin->obj.ptr());
            }
        }
        for (PyObject *obj: held)
            Py_VISIT(obj);
        return 0;
    }

    void clear_retained(const void *owner) {
        std::vector<std::shared_ptr<PyRefPin> > taken;
        {
            std::lock_guard lock(g_pin_mutex);
            const auto found = g_pins.find(owner);
            if (found == g_pins.end())
                return;
            taken.swap(found->second);
            g_pins.erase(found);
        }
        for (auto &pin: taken)
            pin->obj = nb::object();
    }

    void note_python_shutdown() noexcept {
        g_py_shutdown.store(true, std::memory_order_release);
    }

    bool python_shutdown_started() noexcept {
        return g_py_shutdown.load(std::memory_order_acquire);
    }

    void clear_all_retained() {
        std::unordered_map<const void *, std::vector<std::shared_ptr<PyRefPin> > > taken;
        {
            std::lock_guard lock(g_pin_mutex);
            taken.swap(g_pins);
        }
        for (auto &entry: taken) {
            for (auto &pin: entry.second) {
                if (pin)
                    pin->obj = nb::object();
            }
        }
    }

    nb::object json_to_python(const nlohmann::json &value) {
        if (value.is_null())
            return nb::none();
        if (value.is_boolean())
            return nb::cast(value.get<bool>());
        if (value.is_number_unsigned()) {
            const auto number = value.get<uint64_t>();
            if (number > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                return nb::cast(static_cast<double>(number));
            return nb::cast(static_cast<int64_t>(number));
        }
        if (value.is_number_integer())
            return nb::cast(value.get<int64_t>());
        if (value.is_number_float())
            return nb::cast(value.get<double>());
        if (value.is_string())
            return unicode_from_utf8(value.get<std::string>());
        if (value.is_array()) {
            nb::list out;
            for (const auto &item: value)
                out.append(json_to_python(item));
            return out;
        }
        if (value.is_object()) {
            nb::dict out;
            for (auto it = value.begin(); it != value.end(); ++it)
                out[unicode_from_utf8(it.key())] = json_to_python(it.value());
            return out;
        }
        return nb::none();
    }

    nlohmann::json python_to_json(nb::handle value) {
        if (!value.is_valid() || value.is_none())
            return nullptr;

        PyObject *ptr = value.ptr();
        if (PyBool_Check(ptr))
            return nb::cast<bool>(value);
        if (PyLong_Check(ptr))
            return nb::cast<int64_t>(value);
        if (PyFloat_Check(ptr))
            return nb::cast<double>(value);
        if (PyUnicode_Check(ptr))
            return unicode_to_utf8(ptr);

        if (PyList_Check(ptr) || PyTuple_Check(ptr)) {
            nlohmann::json array = nlohmann::json::array();
            const Py_ssize_t count = PySequence_Size(ptr);
            for (Py_ssize_t i = 0; i < count; ++i)
                array.push_back(python_to_json(item_at(value, i)));
            return array;
        }

        if (PyDict_Check(ptr)) {
            nlohmann::json object = nlohmann::json::object();
            PyObject *key = nullptr;
            PyObject *item = nullptr;
            Py_ssize_t pos = 0;
            while (PyDict_Next(ptr, &pos, &key, &item)) {
                if (!PyUnicode_Check(key))
                    throw nb::type_error("JSON object keys must be strings");
                object[unicode_to_utf8(key)] =
                        python_to_json(nb::handle(item));
            }
            return object;
        }

        throw nb::type_error("Value is not JSON-compatible (expected None, bool, "
            "int, float, str, list, or dict)");
    }

    nlohmann::json object_or_empty(nb::handle value) {
        if (!value.is_valid() || value.is_none())
            return nlohmann::json::object();
        nlohmann::json object = python_to_json(value);
        if (!object.is_object())
            throw nb::type_error("Expected a dict");
        return object;
    }

    mb::MbMiddlewares collect_middlewares(nb::handle middlewares,
                                          const void *owner) {
        mb::MbMiddlewares out;
        if (!middlewares.is_valid() || middlewares.is_none())
            return out;

        auto add_one = [&](nb::handle item) {
            if (nb::isinstance<PyMiddleware>(item)) {
                out.push_back(nb::cast<PyMiddleware &>(item).fn);
                return;
            }
            if (!nb::isinstance<nb::callable>(item))
                throw nb::type_error(
                    "middleware must be a callable or a mantisbase.middlewares result");

            const std::shared_ptr<PyRefPin> pin = retain_py_ref(owner, item);
            out.push_back([pin](mb::MbRequest &req, mb::MbResponse &res) {
                nb::gil_scoped_acquire gil;
                const nb::object callback = load_pin(pin);
                if (!callback.is_valid())
                    return mb::MbHandlerResponse::Handled;
                try {
                    nb::object result = nb::borrow<nb::callable>(callback)(&req, &res);
                    if (result.is_none())
                        return mb::MbHandlerResponse::Unhandled;
                    if (nb::isinstance<nb::bool_>(result) && !nb::cast<bool>(result))
                        return mb::MbHandlerResponse::Handled;
                    return mb::MbHandlerResponse::Unhandled;
                } catch (nb::python_error &error) {
                    const std::string message = error.what();
                    error.restore();
                    PyErr_Clear();
                    res.sendJSON(500, nlohmann::json{{"error", message}});
                    return mb::MbHandlerResponse::Handled;
                }
            });
        };

        if (PyList_Check(middlewares.ptr()) || PyTuple_Check(middlewares.ptr())) {
            const Py_ssize_t count = PySequence_Size(middlewares.ptr());
            for (Py_ssize_t i = 0; i < count; ++i)
                add_one(item_at(middlewares, i));
            return out;
        }

        add_one(middlewares);
        return out;
    }

    void register_mantis_error(nb::module_ &m) {
        g_mantis_error =
                PyErr_NewException("mantisbase.MantisError", PyExc_Exception, nullptr);
        if (!g_mantis_error)
            throw std::runtime_error("Failed to create mantisbase.MantisError");

        m.attr("MantisError") = nb::borrow<nb::object>(g_mantis_error);

        nb::register_exception_translator(
            [](const std::exception_ptr &exception, void *) {
                try {
                    std::rethrow_exception(exception);
                } catch (const mb::MbException &error) {
                    Py_INCREF(g_mantis_error);
                    nb::object type = nb::steal<nb::object>(g_mantis_error);
                    // Assigning a std::string here goes through nanobind's
                    // caster, which throws std::bad_cast in a TU that does
                    // not include the string caster. Build the Python
                    // strings directly.
                    nb::object instance = type(unicode_from_utf8(error.what()));
                    instance.attr("code") = error.code();
                    instance.attr("description") = unicode_from_utf8(error.desc());
                    PyErr_SetObject(type.ptr(), instance.ptr());
                }
            });
    }
} // namespace mbpy
