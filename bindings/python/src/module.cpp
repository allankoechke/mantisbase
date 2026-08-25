#include <nanobind/nanobind.h>

namespace nb = nanobind;

void register_app(nb::module_& m);
void register_router(nb::module_& m);
void register_request(nb::module_& m);
void register_response(nb::module_& m);
void register_db(nb::module_& m);

NB_MODULE(mantisbase, m) {
    m.doc() = "Python bindings for mantisbase – Flask-style HTTP server backed by a C++ engine";

    register_app(m);
    register_router(m);
    register_request(m);
    register_response(m);
    register_db(m);
}
