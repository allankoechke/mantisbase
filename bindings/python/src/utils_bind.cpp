#include <mantisbase/utils/utils.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;

void register_utils(nb::module_ &m) {
    nb::module_ utils = m.def_submodule(
        "utils", "Id, password, filename, and environment helpers");
    utils.def("generate_time_based_id", &mb::generateTimeBasedId);
    utils.def("generate_readable_time_id", &mb::generateReadableTimeId);
    utils.def("generate_short_id", &mb::generateShortId,
              nb::arg("length") = static_cast<std::size_t>(16));
    utils.def("get_env", &mb::getEnvOrDefault, nb::arg("key"),
              nb::arg("default") = "");
    utils.def(
        "sanitize_filename",
        [](const std::string &name, std::size_t max_len, std::size_t id_len,
           const std::string &separator) {
            return mb::sanitizeFilename(name, max_len, id_len, separator);
        },
        nb::arg("name"), nb::arg("max_len") = static_cast<std::size_t>(50),
        nb::arg("id_len") = static_cast<std::size_t>(12),
        nb::arg("separator") = "_");
    utils.def("hash_password", &mb::hashPassword, nb::arg("password"));
    utils.def("verify_password", &mb::verifyPassword, nb::arg("password"),
              nb::arg("stored_hash"));
    utils.def("is_allowed_file_extension", &mb::isAllowedFileExtension,
              nb::arg("extension"));
    utils.def("safe_content_type", &mb::safeContentType, nb::arg("extension"));
    utils.def("str_to_bool", &mb::strToBool, nb::arg("value"));
}
