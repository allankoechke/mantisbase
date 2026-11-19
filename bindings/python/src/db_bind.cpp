#include <ctime>
#include <mantisbase/core/database.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <soci/soci.h>

#include "py_conv.h"

namespace nb = nanobind;

namespace {
    nlohmann::json soci_row_to_json(const soci::row &row) {
        nlohmann::json object = nlohmann::json::object();
        for (std::size_t i = 0; i < row.size(); ++i) {
            const auto &props = row.get_properties(i);
            const std::string &name = props.get_name();
            if (row.get_indicator(i) == soci::i_null) {
                object[name] = nullptr;
                continue;
            }

            switch (props.get_db_type()) {
                case soci::db_string:
                case soci::db_xml:
                    object[name] = row.get<std::string>(i);
                    break;
                case soci::db_double:
                    object[name] = row.get<double>(i);
                    break;
                case soci::db_int8:
                    object[name] = static_cast<int64_t>(row.get<int8_t>(i));
                    break;
                case soci::db_uint8:
                    object[name] = static_cast<int64_t>(row.get<uint8_t>(i));
                    break;
                case soci::db_int16:
                    object[name] = static_cast<int64_t>(row.get<int16_t>(i));
                    break;
                case soci::db_uint16:
                    object[name] = static_cast<int64_t>(row.get<uint16_t>(i));
                    break;
                case soci::db_int32:
                    object[name] = static_cast<int64_t>(row.get<int32_t>(i));
                    break;
                case soci::db_uint32:
                    object[name] = static_cast<int64_t>(row.get<uint32_t>(i));
                    break;
                case soci::db_int64:
                    object[name] = row.get<long long>(i);
                    break;
                case soci::db_uint64:
                    object[name] = static_cast<int64_t>(row.get<unsigned long long>(i));
                    break;
                case soci::db_date: {
                    const auto time = row.get<std::tm>(i);
                    char buffer[32];
                    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &time);
                    object[name] = std::string(buffer);
                    break;
                }
                default:
                    object[name] = row.get<std::string>(i);
                    break;
            }
        }
        return object;
    }

    soci::values binds_from(const nb::args &params, bool &has_binds) {
        soci::values binds;
        has_binds = false;
        for (nb::handle param: params) {
            if (!PyDict_Check(param.ptr()))
                throw nb::type_error("Bind parameters must be dicts");
            has_binds = true;
            PyObject *key = nullptr;
            PyObject *value = nullptr;
            Py_ssize_t pos = 0;
            while (PyDict_Next(param.ptr(), &pos, &key, &value)) {
                if (!PyUnicode_Check(key))
                    throw nb::type_error("Bind names must be strings");
                const std::string name = nb::cast<std::string>(nb::handle(key));
                if (value == Py_None) {
                    std::optional<int> null_value;
                    binds.set(name, null_value, soci::i_null);
                } else if (PyBool_Check(value)) {
                    binds.set(name, nb::cast<bool>(nb::handle(value)));
                } else if (PyLong_Check(value)) {
                    binds.set(name, nb::cast<long long>(nb::handle(value)));
                } else if (PyFloat_Check(value)) {
                    binds.set(name, nb::cast<double>(nb::handle(value)));
                } else if (PyUnicode_Check(value)) {
                    binds.set(name, nb::cast<std::string>(nb::handle(value)));
                } else {
                    throw nb::type_error(
                        "Bind values must be None, bool, int, float, or str");
                }
            }
        }
        return binds;
    }

    nb::list run_query(soci::session &session, const std::string &sql,
                       const nb::args &params) {
        bool has_binds = false;
        soci::values binds = binds_from(params, has_binds);

        nlohmann::json results;
        {
            nb::gil_scoped_release release;
            soci::row row;
            soci::statement statement =
                    has_binds
                        ? (session.prepare << sql, soci::use(binds), soci::into(row))
                        : (session.prepare << sql, soci::into(row));
            statement.execute();
            results = nlohmann::json::array();
            while (statement.fetch())
                results.push_back(soci_row_to_json(row));
        }

        nb::list out;
        for (const auto &row: results)
            out.append(mbpy::json_to_python(row));
        return out;
    }

    class PyTransaction {
    public:
        explicit PyTransaction(mb::MbDatabase &database) {
            nb::gil_scoped_release release;
            m_session = database.session();
            m_session->begin();
        }

        PyTransaction(const PyTransaction &) = delete;

        PyTransaction &operator=(const PyTransaction &) = delete;

        PyTransaction(PyTransaction &&other) noexcept
            : m_session(std::move(other.m_session)), m_done(other.m_done) {
            other.m_done = true;
        }

        PyTransaction &operator=(PyTransaction &&) = delete;

        ~PyTransaction() {
            if (!m_done && m_session) {
                nb::gil_scoped_release release;
                try {
                    m_session->rollback();
                } catch (...) {
                }
            }
        }

        void commit() {
            nb::gil_scoped_release release;
            m_session->commit();
            m_done = true;
        }

        void rollback() {
            nb::gil_scoped_release release;
            m_session->rollback();
            m_done = true;
        }

        nb::list query(const std::string &sql, const nb::args &params) {
            return run_query(*m_session, sql, params);
        }

        bool exit(nb::handle exc_type) {
            if (exc_type.is_none())
                commit();
            else
                rollback();
            return false;
        }

    private:
        std::shared_ptr<soci::session> m_session;
        bool m_done = false;
    };
} // namespace

void register_db(nb::module_ &m) {
    nb::class_<PyTransaction>(
                m, "Transaction", "Database transaction leased from the connection pool")
            .def(
                "query",
                [](PyTransaction &self, const std::string &sql,
                   const nb::args &params) {
                    return self.query(sql, params);
                },
                nb::arg("sql"), nb::arg("params"))
            .def("commit", &PyTransaction::commit)
            .def("rollback", &PyTransaction::rollback)
            .def(
                "__enter__",
                [](PyTransaction &self) -> PyTransaction & { return self; },
                nb::rv_policy::reference)
            .def(
                "__exit__",
                [](PyTransaction &self, nb::handle exc_type, nb::handle, nb::handle) {
                    return self.exit(exc_type);
                },
                nb::arg("exc_type"), nb::arg("exc"), nb::arg("traceback"));

    nb::class_<mb::MbDatabase>(m, "MbDatabase", "Pooled SQL connection")
            .def(
                "query",
                [](mb::MbDatabase &self, const std::string &sql,
                   const nb::args &params) {
                    auto session = self.session();
                    return run_query(*session, sql, params);
                },
                nb::arg("sql"), nb::arg("params"))
            .def(
                "transaction",
                [](mb::MbDatabase &self) { return PyTransaction(self); },
                nb::keep_alive<0, 1>())
            .def_prop_ro("connected", &mb::MbDatabase::isConnected)
            .def_prop_ro("connection_string", &mb::MbDatabase::connectionStr);
}
