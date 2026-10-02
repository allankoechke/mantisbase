#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/core/http.h>
#include <nlohmann/json.hpp>

namespace nb = nanobind;
using json = nlohmann::json;

static nb::object json_to_python(const json& j) {
    if (j.is_null()) return nb::none();
    if (j.is_boolean()) return nb::cast(j.get<bool>());
    if (j.is_number_integer()) return nb::cast(j.get<int64_t>());
    if (j.is_number_float()) return nb::cast(j.get<double>());
    if (j.is_string()) return nb::cast(j.get<std::string>());
    if (j.is_array()) {
        nb::list lst;
        for (const auto& elem : j)
            lst.append(json_to_python(elem));
        return lst;
    }
    if (j.is_object()) {
        nb::dict d;
        for (auto it = j.begin(); it != j.end(); ++it)
            d[nb::cast(it.key())] = json_to_python(it.value());
        return d;
    }
    return nb::none();
}

void register_request(const nb::module_& m) {
    nb::class_<mb::MbRequest>(m, "MbRequest")
        .def("path_param", [](const mb::MbRequest& self, const std::string& name) {
            return self.getPathParamValue(name);
        }, nb::arg("name"))
        .def("query_param", [](const mb::MbRequest& self, const std::string& name) {
            return self.getQueryParamValue(name);
        }, nb::arg("name"))
        .def("header", [](const mb::MbRequest& self, const std::string& name) {
            return self.getHeaderValue(name);
        }, nb::arg("name"))
        .def("body", &mb::MbRequest::getBody)
        .def("json", [](const mb::MbRequest& self) -> nb::object {
            try {
                const json j = self.jsonBody();
                return json_to_python(j);
            } catch (const std::exception& e) {
                throw nb::value_error(e.what());
            }
        })
        .def_prop_ro("method", &mb::MbRequest::getMethod)
        .def_prop_ro("path", &mb::MbRequest::getPath)
        .def_prop_ro("remote_addr", &mb::MbRequest::getRemoteAddr);
}
