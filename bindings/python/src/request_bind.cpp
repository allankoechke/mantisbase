#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <any>
#include <typeinfo>
#include <mantisbase/core/http.h>
#include <mantisbase/core/models/entity.h>

#include "py_conv.h"

namespace nb = nanobind;

namespace {

nb::object text_or_bytes(const std::string &value) {
    PyObject *text = PyUnicode_DecodeUTF8(value.data(), static_cast<Py_ssize_t>(value.size()), nullptr);
    if (text)
        return nb::steal<nb::object>(text);
    PyErr_Clear();
    return nb::steal<nb::object>(PyBytes_FromStringAndSize(value.data(), static_cast<Py_ssize_t>(value.size())));
}

nb::object attribute_value(mb::MbRequest &request, const std::string &key) {
    if (!request.hasKey(key))
        return nb::none();

    const std::any &stored = (*request.drogonRequest()->attributes())[key];
    if (!stored.has_value())
        return nb::none();
    if (stored.type() == typeid(nlohmann::json))
        return mbpy::json_to_python(std::any_cast<const nlohmann::json &>(stored));
    if (stored.type() == typeid(std::string))
        return nb::cast(std::any_cast<const std::string &>(stored));
    if (stored.type() == typeid(bool))
        return nb::cast(std::any_cast<bool>(stored));
    if (stored.type() == typeid(int))
        return nb::cast(std::any_cast<int>(stored));
    if (stored.type() == typeid(int64_t))
        return nb::cast(std::any_cast<int64_t>(stored));
    if (stored.type() == typeid(double))
        return nb::cast(std::any_cast<double>(stored));

    throw nb::type_error("Request attribute is not a JSON-compatible value");
}

} // namespace

void register_request(nb::module_ &m) {
    nb::class_<mb::MbRequest>(m, "MbRequest", "Incoming HTTP request passed to a route handler")
        .def("path_param", &mb::MbRequest::pathParamValue, nb::arg("name"))
        .def("has_path_params", &mb::MbRequest::hasPathParams)
        .def("has_path_param", &mb::MbRequest::hasPathParam, nb::arg("name"))
        .def("query_param", &mb::MbRequest::queryParamValue, nb::arg("name"))
        .def("has_query_param", &mb::MbRequest::hasQueryParam, nb::arg("name"))
        .def("header", &mb::MbRequest::headerValue, nb::arg("name"), nb::arg("default") = "")
        .def("has_header", &mb::MbRequest::hasHeader, nb::arg("name"))
        .def("hasHeader", &mb::MbRequest::hasHeader, nb::arg("name"))
        .def("cookie", &mb::MbRequest::cookieValue, nb::arg("name"))
        .def("bearer_token", &mb::MbRequest::bearerTokenAuth)
        .def("auth_token", &mb::MbRequest::resolveAuthToken)
        .def("body", &mb::MbRequest::body)
        .def("json", [](const mb::MbRequest &self) { return mbpy::json_to_python(self.jsonBody()); })
        .def("is_multipart", &mb::MbRequest::isMultipartFormData)
        .def("form_data", [](const mb::MbRequest &self) {
            nb::list parts;
            for (const auto &part : self.formData()) {
                nb::dict item;
                item["name"] = part.name;
                item["filename"] = part.filename;
                item["content_type"] = part.content_type;
                item["content"] = text_or_bytes(part.content);
                parts.append(std::move(item));
            }
            return parts;
        })
        .def("file_metadata", [](const mb::MbRequest &self) { return mbpy::json_to_python(self.filesMetadata()); })
        .def("parse_form", [](const mb::MbRequest &self, const mb::MbEntity &entity) {
            self.parseFormDataToEntity(entity);
        }, nb::arg("entity"), "Map a multipart body onto an entity. No-op for JSON bodies.")
        .def("write_files", &mb::MbRequest::writeFiles, nb::arg("entity"))
        .def("undo_files", &mb::MbRequest::undoWrittenFiles, nb::arg("entity"))
        .def("has_key", &mb::MbRequest::hasKey, nb::arg("key"))
        .def("get", [](mb::MbRequest &self, const std::string &key) { return attribute_value(self, key); }, nb::arg("key"))
        .def("get_or", [](mb::MbRequest &self, const std::string &key, nb::handle fallback) {
            if (!self.hasKey(key)) {
                Py_INCREF(fallback.ptr());
                return nb::steal<nb::object>(fallback.ptr());
            }
            return attribute_value(self, key);
        }, nb::arg("key"), nb::arg("default"))
        .def("set", [](mb::MbRequest &self, const std::string &key, nb::handle value) {
            self.set<nlohmann::json>(key, mbpy::python_to_json(value));
        }, nb::arg("key"), nb::arg("value"))
        .def("is_guest", &mb::MbRequest::isGuestAuth)
        .def("is_admin", &mb::MbRequest::isAdminAuth)
        .def("is_user", &mb::MbRequest::isUserAuth)
        .def_prop_ro("method", &mb::MbRequest::method)
        .def_prop_ro("path", &mb::MbRequest::path)
        .def_prop_ro("remote_addr", &mb::MbRequest::remoteAddr)
        .def_prop_ro("remote_port", &mb::MbRequest::remotePort)
        .def_prop_ro("local_addr", &mb::MbRequest::localAddr)
        .def_prop_ro("local_port", &mb::MbRequest::localPort);
}
