#include <mantisbase/core/models/entity.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "py_conv.h"

namespace nb = nanobind;

namespace {
    nlohmann::json opts_of(nb::handle opts) { return mbpy::object_or_empty(opts); }

    nb::object optional_record(const std::optional<nlohmann::json> &record) {
        if (!record.has_value())
            return nb::none();
        return mbpy::json_to_python(*record);
    }

    nb::list records_to_list(const mb::Records &records) {
        nb::list out;
        for (const auto &record: records)
            out.append(mbpy::json_to_python(record));
        return out;
    }
} // namespace

void register_entity(nb::module_ &m) {
    nb::class_<mb::MbEntity>(m, "Entity",
                             "One database table and its CRUD operations")
            .def_prop_ro("id", &mb::MbEntity::id)
            .def_prop_ro("name", &mb::MbEntity::name)
            .def_prop_ro("type", &mb::MbEntity::type)
            .def_prop_ro("is_system", &mb::MbEntity::isSystem)
            .def_prop_ro("has_api", &mb::MbEntity::hasApi)
            .def_prop_ro("schema",
                         [](const mb::MbEntity &self) {
                             return mbpy::json_to_python(self.schema());
                         })
            .def_prop_ro("rules",
                         [](const mb::MbEntity &self) {
                             return mbpy::json_to_python(self.rules());
                         })
            .def_prop_ro("fields",
                         [](const mb::MbEntity &self) {
                             return mbpy::json_to_python(nlohmann::json(self.fields()));
                         })
            .def(
                "field",
                [](const mb::MbEntity &self, const std::string &name) {
                    return optional_record(self.field(name));
                },
                nb::arg("name"))
            .def(
                "has_field",
                [](const mb::MbEntity &self, const std::string &name) {
                    return self.hasField(name).has_value();
                },
                nb::arg("name"))
            .def("auth_methods",
                 [](const mb::MbEntity &self) {
                     return mbpy::json_to_python(self.authMethods().toJSON());
                 })
            .def("auth_method_disabled_reason",
                 &mb::MbEntity::authMethodDisabledReason, nb::arg("method"))
            .def("view_query", &mb::MbEntity::viewQuery)
            .def("list_rule",
                 [](const mb::MbEntity &self) {
                     return mbpy::json_to_python(self.listRule().toJSON());
                 })
            .def("get_rule",
                 [](const mb::MbEntity &self) {
                     return mbpy::json_to_python(self.getRule().toJSON());
                 })
            .def("add_rule",
                 [](const mb::MbEntity &self) {
                     return mbpy::json_to_python(self.addRule().toJSON());
                 })
            .def("update_rule",
                 [](const mb::MbEntity &self) {
                     return mbpy::json_to_python(self.updateRule().toJSON());
                 })
            .def("delete_rule",
                 [](const mb::MbEntity &self) {
                     return mbpy::json_to_python(self.deleteRule().toJSON());
                 })
            .def(
                "create",
                [](const mb::MbEntity &self, nb::handle record, nb::handle opts) {
                    const nlohmann::json body = mbpy::python_to_json(record);
                    const nlohmann::json options = opts_of(opts);
                    return mbpy::call_json([&] { return self.create(body, options); });
                },
                nb::arg("record"), nb::arg("opts") = nb::none())
            .def(
                "list",
                [](const mb::MbEntity &self, nb::handle opts) {
                    const nlohmann::json options = opts_of(opts);
                    mb::Records rows;
                    {
                        nb::gil_scoped_release release;
                        rows = self.list(options);
                    }
                    return records_to_list(rows);
                },
                nb::arg("opts") = nb::none())
            .def(
                "list_page",
                [](const mb::MbEntity &self, nb::handle opts) {
                    const nlohmann::json options = opts_of(opts);
                    mb::EntityListPage page;
                    {
                        nb::gil_scoped_release release;
                        page = self.listPage(options);
                    }
                    nb::dict out;
                    out["items"] = records_to_list(page.items);
                    out["has_more"] = page.has_more;
                    return out;
                },
                nb::arg("opts") = nb::none())
            .def(
                "read",
                [](const mb::MbEntity &self, const std::string &id, nb::handle opts) {
                    const nlohmann::json options = opts_of(opts);
                    std::optional<mb::Record> record;
                    {
                        nb::gil_scoped_release release;
                        record = self.read(id, options);
                    }
                    return optional_record(record);
                },
                nb::arg("id"), nb::arg("opts") = nb::none())
            .def(
                "update",
                [](const mb::MbEntity &self, const std::string &id, nb::handle data,
                   nb::handle opts) {
                    const nlohmann::json body = mbpy::python_to_json(data);
                    const nlohmann::json options = opts_of(opts);
                    return mbpy::call_json(
                        [&] { return self.update(id, body, options); });
                },
                nb::arg("id"), nb::arg("data"), nb::arg("opts") = nb::none())
            .def(
                "remove",
                [](const mb::MbEntity &self, const std::string &id) {
                    nb::gil_scoped_release release;
                    self.remove(id);
                },
                nb::arg("id"))
            .def(
                "delete",
                [](const mb::MbEntity &self, const std::string &id) {
                    nb::gil_scoped_release release;
                    self.remove(id);
                },
                nb::arg("id"))
            .def(
                "count",
                [](const mb::MbEntity &self, nb::handle filter) {
                    const nlohmann::json query = opts_of(filter);
                    nb::gil_scoped_release release;
                    return self.countRecords(query);
                },
                nb::arg("filter") = nb::none())
            .def("is_empty",
                 [](const mb::MbEntity &self) {
                     nb::gil_scoped_release release;
                     return self.isEmpty();
                 })
            .def(
                "record_exists",
                [](const mb::MbEntity &self, const std::string &id) {
                    nb::gil_scoped_release release;
                    return self.recordExists(id);
                },
                nb::arg("id"))
            .def(
                "find_by_columns",
                [](const mb::MbEntity &self, const std::string &value,
                   nb::handle columns) {
                    std::vector<std::string> names;
                    nlohmann::json listed = mbpy::python_to_json(columns);
                    if (!listed.is_array())
                        throw nb::type_error("columns must be a list of field names");
                    for (const auto &name: listed) {
                        if (!name.is_string())
                            throw nb::type_error("columns must be a list of field names");
                        names.push_back(name.get<std::string>());
                    }
                    std::optional<nlohmann::json> record;
                    {
                        nb::gil_scoped_release release;
                        record = self.queryFromCols(value, names);
                    }
                    return optional_record(record);
                },
                nb::arg("value"), nb::arg("columns"));
}
