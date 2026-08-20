#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/core/database.h>
#include <soci/soci.h>
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

static json soci_row_to_json(const soci::row& row) {
    json obj = json::object();
    for (size_t i = 0; i < row.size(); ++i) {
        const auto& props = row.get_properties(i);
        const std::string& name = props.get_name();

        if (row.get_indicator(i) == soci::i_null) {
            obj[name] = nullptr;
            continue;
        }

        switch (props.get_db_type()) {
            case soci::db_string:
            case soci::db_xml:
                obj[name] = row.get<std::string>(i);
                break;
            case soci::db_double:
                obj[name] = row.get<double>(i);
                break;
            case soci::db_int8:
                obj[name] = static_cast<int64_t>(row.get<int8_t>(i));
                break;
            case soci::db_uint8:
                obj[name] = static_cast<int64_t>(row.get<uint8_t>(i));
                break;
            case soci::db_int16:
                obj[name] = static_cast<int64_t>(row.get<int16_t>(i));
                break;
            case soci::db_uint16:
                obj[name] = static_cast<int64_t>(row.get<uint16_t>(i));
                break;
            case soci::db_int32:
                obj[name] = static_cast<int64_t>(row.get<int32_t>(i));
                break;
            case soci::db_uint32:
                obj[name] = static_cast<int64_t>(row.get<uint32_t>(i));
                break;
            case soci::db_int64:
                obj[name] = static_cast<int64_t>(row.get<int64_t>(i));
                break;
            case soci::db_uint64:
                obj[name] = static_cast<int64_t>(row.get<uint64_t>(i));
                break;
            case soci::db_date: {
                std::tm t = row.get<std::tm>(i);
                char buf[32];
                std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
                obj[name] = std::string(buf);
                break;
            }
            default:
                obj[name] = row.get<std::string>(i);
                break;
        }
    }
    return obj;
}

void register_db(nb::module_& m) {
    nb::class_<mb::Database>(m, "Database")
        .def("query", [](mb::Database& self, const std::string& sql, nb::args params) -> nb::list {
            soci::values binds;
            bool has_binds = false;

            for (size_t i = 0; i < params.size(); ++i) {
                nb::handle param = params[i];
                if (!nb::isinstance<nb::dict>(param))
                    throw nb::type_error("Bind parameters must be dicts");

                has_binds = true;
                nb::dict d = nb::cast<nb::dict>(param);
                for (auto item : d) {
                    std::string key = nb::cast<std::string>(item.first);
                    nb::handle val = item.second;

                    if (val.is_none()) {
                        std::optional<int> null_val;
                        binds.set(key, null_val, soci::i_null);
                    } else if (nb::isinstance<nb::str>(val)) {
                        binds.set(key, nb::cast<std::string>(val));
                    } else if (nb::isinstance<nb::bool_>(val)) {
                        binds.set(key, nb::cast<bool>(val));
                    } else if (nb::isinstance<nb::int_>(val)) {
                        binds.set(key, nb::cast<int>(val));
                    } else if (nb::isinstance<nb::float_>(val)) {
                        binds.set(key, nb::cast<double>(val));
                    }
                }
            }

            json results;
            {
                nb::gil_scoped_release release;
                auto session = self.session();

                soci::row data_row;
                soci::statement st = has_binds
                    ? (session->prepare << sql, soci::use(binds), soci::into(data_row))
                    : (session->prepare << sql, soci::into(data_row));
                st.execute();

                results = json::array();
                while (st.fetch()) {
                    results.push_back(soci_row_to_json(data_row));
                }
            }

            nb::list py_results;
            for (const auto& row : results) {
                py_results.append(json_to_python(row));
            }
            return py_results;
        }, nb::arg("sql"))
        .def_prop_ro("connected", &mb::Database::isConnected);
}
