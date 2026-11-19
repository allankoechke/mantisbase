#include "py_conv.h"

#include <limits>
#include <mantisbase/core/http.h>

namespace mbpy {
namespace {

PyObject *g_mantis_error = nullptr;

nb::object item_at(nb::handle sequence, Py_ssize_t index) {
    return nb::steal<nb::object>(PySequence_GetItem(sequence.ptr(), index));
}

} // namespace

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
        return nb::cast(value.get<std::string>());
    if (value.is_array()) {
        nb::list out;
        for (const auto &item : value)
            out.append(json_to_python(item));
        return out;
    }
    if (value.is_object()) {
        nb::dict out;
        for (auto it = value.begin(); it != value.end(); ++it)
            out[nb::cast(it.key())] = json_to_python(it.value());
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
        return nb::cast<std::string>(value);

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
            object[nb::cast<std::string>(nb::handle(key))] = python_to_json(nb::handle(item));
        }
        return object;
    }

    throw nb::type_error("Value is not JSON-compatible (expected None, bool, int, float, str, list, or dict)");
}

nlohmann::json object_or_empty(nb::handle value) {
    if (!value.is_valid() || value.is_none())
        return nlohmann::json::object();
    nlohmann::json object = python_to_json(value);
    if (!object.is_object())
        throw nb::type_error("Expected a dict");
    return object;
}

mb::MbMiddlewares collect_middlewares(nb::handle middlewares) {
    mb::MbMiddlewares out;
    if (!middlewares.is_valid() || middlewares.is_none())
        return out;

    auto add_one = [&](nb::handle item) {
        if (nb::isinstance<PyMiddleware>(item)) {
            out.push_back(nb::cast<PyMiddleware &>(item).fn);
            return;
        }
        if (!nb::isinstance<nb::callable>(item))
            throw nb::type_error("middleware must be a callable or a mantisbase.middlewares result");

        nb::callable callback = nb::cast<nb::callable>(item);
        out.push_back([callback](mb::MbRequest &req, mb::MbResponse &res) {
            nb::gil_scoped_acquire gil;
            try {
                nb::object result = callback(&req, &res);
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
    g_mantis_error = PyErr_NewException("mantisbase.MantisError", PyExc_Exception, nullptr);
    if (!g_mantis_error)
        throw std::runtime_error("Failed to create mantisbase.MantisError");

    m.attr("MantisError") = nb::borrow<nb::object>(g_mantis_error);

    nb::register_exception_translator([](const std::exception_ptr &exception, void *) {
        try {
            std::rethrow_exception(exception);
        } catch (const mb::MbException &error) {
            Py_INCREF(g_mantis_error);
            nb::object type = nb::steal<nb::object>(g_mantis_error);
            nb::object instance = type(std::string(error.what()));
            instance.attr("code") = error.code();
            instance.attr("description") = std::string(error.desc());
            PyErr_SetObject(type.ptr(), instance.ptr());
        }
    });
}

} // namespace mbpy
