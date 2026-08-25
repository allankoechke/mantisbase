#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/core/http.h>
#include <nlohmann/json.hpp>

namespace nb = nanobind;
using json = nlohmann::json;

static json python_to_json(nb::handle obj) {
    if (obj.is_none()) return nullptr;
    if (nb::isinstance<nb::bool_>(obj)) return nb::cast<bool>(obj);
    if (nb::isinstance<nb::int_>(obj)) return nb::cast<int64_t>(obj);
    if (nb::isinstance<nb::float_>(obj)) return nb::cast<double>(obj);
    if (nb::isinstance<nb::str>(obj)) return nb::cast<std::string>(obj);
    if (nb::isinstance<nb::list>(obj)) {
        json arr = json::array();
        for (auto item : obj)
            arr.push_back(python_to_json(item));
        return arr;
    }
    if (nb::isinstance<nb::dict>(obj)) {
        json d = json::object();
        for (auto item : nb::cast<nb::dict>(obj))
            d[nb::cast<std::string>(item.first)] = python_to_json(item.second);
        return d;
    }
    return nullptr;
}

void register_response(nb::module_& m) {
    nb::class_<mb::MantisResponse>(m, "MantisResponse")
        .def("json", [](mb::MantisResponse& self, int status, nb::handle data) {
            self.sendJSON(status, python_to_json(data));
        }, nb::arg("status"), nb::arg("data"))
        .def("html", &mb::MantisResponse::sendHtml, nb::arg("status"), nb::arg("body"))
        .def("text", &mb::MantisResponse::sendText, nb::arg("status"), nb::arg("body"))
        .def("send", &mb::MantisResponse::send,
             nb::arg("status"), nb::arg("data") = "", nb::arg("content_type") = "text/plain")
        .def("redirect", &mb::MantisResponse::setRedirect,
             nb::arg("url"), nb::arg("status") = 302)
        .def("set_header", &mb::MantisResponse::setHeader,
             nb::arg("name"), nb::arg("value"));
}
