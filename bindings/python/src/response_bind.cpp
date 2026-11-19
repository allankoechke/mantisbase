#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/core/http.h>

#include "py_conv.h"

namespace nb = nanobind;

void register_response(nb::module_ &m) {
    nb::class_<mb::MbResponse>(m, "MbResponse", "Outgoing HTTP response passed to a route handler")
        .def("json", [](const mb::MbResponse &self, int status, nb::handle data) {
            self.sendJSON(status, mbpy::python_to_json(data));
        }, nb::arg("status"), nb::arg("data"))
        .def("html", &mb::MbResponse::sendHtml, nb::arg("status"), nb::arg("body"))
        .def("text", &mb::MbResponse::sendText, nb::arg("status"), nb::arg("body"))
        .def("send", &mb::MbResponse::send, nb::arg("status"), nb::arg("data") = "", nb::arg("content_type") = "text/plain")
        .def("empty", &mb::MbResponse::sendEmpty, nb::arg("status") = 204)
        .def("redirect", &mb::MbResponse::setRedirect, nb::arg("url"), nb::arg("status") = 302)
        .def("set_header", &mb::MbResponse::setHeader, nb::arg("name"), nb::arg("value"))
        .def("has_header", &mb::MbResponse::hasHeader, nb::arg("name"))
        .def("header", &mb::MbResponse::headerValue, nb::arg("name"), nb::arg("default") = "")
        .def("set_content", [](const mb::MbResponse &self, const std::string &body, const std::string &content_type) {
            self.setContent(body, content_type);
        }, nb::arg("body"), nb::arg("content_type"))
        .def("set_file", [](const mb::MbResponse &self, const std::string &path, nb::handle content_type) {
            if (content_type.is_none())
                self.setFileContent(path);
            else
                self.setFileContent(path, nb::cast<std::string>(content_type));
        }, nb::arg("path"), nb::arg("content_type") = nb::none())
        .def("set_auth_cookie", &mb::MbResponse::setAuthTokenCookie, nb::arg("token"), nb::arg("max_age"))
        .def("clear_auth_cookie", &mb::MbResponse::clearAuthTokenCookie)
        .def_prop_rw("status", &mb::MbResponse::getStatus, &mb::MbResponse::setStatus)
        .def_prop_rw("body", &mb::MbResponse::body, &mb::MbResponse::setBody)
        .def_prop_ro("location", &mb::MbResponse::location);
}
