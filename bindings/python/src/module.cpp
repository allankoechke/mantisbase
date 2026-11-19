#include <mantisbase/mantisbase.h>
#include <nanobind/nanobind.h>

#include "py_conv.h"

namespace nb = nanobind;

void register_entity(nb::module_ & m);
void register_request(nb::module_ & m);
void register_response(nb::module_ & m);
void register_middleware(nb::module_ & m);
void register_db(nb::module_ & m);
void register_auth(nb::module_ & m);
void register_services(nb::module_ & m);
void register_router(nb::module_ & m);
void register_app(nb::module_ & m);
void register_utils(nb::module_ & m);

NB_MODULE(mantisbase, m) {
    m.doc() = "Python bindings for MantisBase, a C++20 backend-as-a-service";

    mbpy::register_mantis_error(m);
    register_entity(m);
    register_request(m);
    register_response(m);
    register_middleware(m);
    register_db(m);
    register_auth(m);
    register_services(m);
    register_router(m);
    register_app(m);
    register_utils(m);

    m.def("version", &mb::MantisBase::appVersion, "MantisBase version string");

    // Names used by earlier examples, alongside the Mb* classes.
    m.attr("Router") = m.attr("MbRouter");
    m.attr("MantisRequest") = m.attr("MbRequest");
    m.attr("MantisResponse") = m.attr("MbResponse");
    m.attr("Database") = m.attr("MbDatabase");
}
