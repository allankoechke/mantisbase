#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/core/http.h>
#include <nlohmann/json.hpp>

namespace nb = nanobind;
using json = nlohmann::json;

static nb::object json_to_python(const json &j) {
    if (j.is_null()) return nb::none();
    if (j.is_boolean()) return nb::cast(j.get<bool>());
    if (j.is_number_integer()) return nb::cast(j.get<int64_t>());
    if (j.is_number_float()) return nb::cast(j.get<double>());
    if (j.is_string()) return nb::cast(j.get<std::string>());
    if (j.is_array()) {
        nb::list lst;
        for (const auto &elem: j)
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

void register_request(const nb::module_ &m) {
    nb::class_<mb::MbRequest>(m, "MbRequest")
            .def("path_param", [](const mb::MbRequest &self, const std::string &name) {
                return self.pathParamValue(name);
            }, nb::arg("name"))
            .def("has_path_params", [](const mb::MbRequest &self) {
                return self.hasPathParams();
            })
            .def("has_path_param", [](const mb::MbRequest &self, const std::string &name) {
                return self.hasPathParam(name);
            }, nb::arg("name"))
            .def("query_param", [](const mb::MbRequest &self, const std::string &name) {
                return self.queryParamValue(name);
            }, nb::arg("name"))
            .def("header", [](const mb::MbRequest &self, const std::string &name) {
                return self.headerValue(name);
            }, nb::arg("name"))
            .def("hasHeader", [](const mb::MbRequest &self, const std::string &name) {
                return self.hasHeader(name);
            }, nb::arg("name"))
            .def("body", &mb::MbRequest::body)
            .def("json", [](const mb::MbRequest &self) -> nb::object {
                try {
                    const json j = self.jsonBody();
                    return json_to_python(j);
                } catch (const std::exception &e) {
                    throw nb::value_error(e.what());
                }
            })
            .def_prop_ro("method", &mb::MbRequest::method)
            .def_prop_ro("path", &mb::MbRequest::path)
            .def_prop_ro("remote_addr", &mb::MbRequest::remoteAddr)
            .def_prop_ro("remote_port", &mb::MbRequest::remotePort)
            .def_prop_ro("local_addr", &mb::MbRequest::localAddr)
            .def_prop_ro("local_port", &mb::MbRequest::localPort);
}
