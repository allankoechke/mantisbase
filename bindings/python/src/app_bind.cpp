#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mantisbase/core/models/entity_schema.h>
#include <mantisbase/mantisbase.h>
#include <memory>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <optional>
#include <thread>

#include "py_conv.h"

namespace nb = nanobind;
using json = nlohmann::json;

namespace {
    class PyApp {
    public:
        explicit PyApp(const nb::kwargs &kwargs) {
            json config = json::object();
            config["serve"] = json::object();

            if (kwargs.contains("port"))
                config["serve"]["port"] = nb::cast<int>(kwargs["port"]);
            if (kwargs.contains("host"))
                config["serve"]["host"] = nb::cast<std::string>(kwargs["host"]);
            if (kwargs.contains("data_dir"))
                config["data-dir"] = nb::cast<std::string>(kwargs["data_dir"]);
            if (kwargs.contains("db_type"))
                config["db"] = nb::cast<std::string>(kwargs["db_type"]);
            if (kwargs.contains("db_url"))
                config["db_url"] = nb::cast<std::string>(kwargs["db_url"]);
            if (kwargs.contains("public_dir"))
                config["public-dir"] = nb::cast<std::string>(kwargs["public_dir"]);
            if (kwargs.contains("scripts_dir"))
                config["scripts-dir"] = nb::cast<std::string>(kwargs["scripts_dir"]);
            if (kwargs.contains("migrations_dir"))
                config["migrations-dir"] =
                        nb::cast<std::string>(kwargs["migrations_dir"]);
            if (kwargs.contains("pool_size"))
                config["serve"]["pool-size"] = nb::cast<int>(kwargs["pool_size"]);
            if (kwargs.contains("dev") && nb::cast<bool>(kwargs["dev"]))
                config["dev"] = true;
            if (kwargs.contains("skip_admin_setup"))
                config["serve"]["skip-admin-setup"] =
                        nb::cast<bool>(kwargs["skip_admin_setup"]);

            if (kwargs.contains("secret_key")) {
                const auto secret = nb::cast<std::string>(kwargs["secret_key"]);
#ifdef _WIN32
                _putenv_s("MB_JWT_SECRET", secret.c_str());
#else
                ::setenv("MB_JWT_SECRET", secret.c_str(), 1);
#endif
            }

            m_app = mb::MantisBase::create(config);
            if (!m_app)
                throw std::runtime_error("Failed to create MantisBase instance");
            m_pin_key = &m_app->router();

            if (kwargs.contains("templates_dir"))
                m_app->setTemplatesDir(nb::cast<std::string>(kwargs["templates_dir"]));
        }

        ~PyApp() {
            // stop() drops the pinned callables after the server thread is
            // joined, which breaks the App ↔ closure cycle while this object
            // is still reachable from Python. Leaving the cycle for interpreter
            // finalization faults on CPython 3.10, 3.13, and 3.14.
            stop();
            m_pin_key = nullptr;
        }

        [[nodiscard]] const void *pin_key() const { return m_pin_key; }

        void start(bool blocking) {
            if (m_running.load())
                return;

            m_running.store(true);
            if (blocking) {
                nb::gil_scoped_release release;
                [[maybe_unused]] const int code = m_app->run();
                m_running.store(false);
            } else {
                m_serverThread = std::thread([this]() {
                    [[maybe_unused]] const int code = m_app->run();
                    m_running.store(false);
                });
            }
        }

        void stop() {
            if (!m_app)
                return;

            if (m_running.load() || m_serverThread.joinable()) {
                // Releasing the GIL while the interpreter is finalizing faults
                // on CPython 3.10, 3.13, and 3.14. The server thread is blocked
                // in the event loop, so close() and join() can run with the
                // GIL held. An in-request stop() is not in that window and
                // still drops the GIL so a handler can finish.
                const bool release_gil =
                        Py_IsInitialized() && !mbpy::python_shutdown_started();
                std::optional<nb::gil_scoped_release> release;
                if (release_gil)
                    release.emplace();
                if (m_running.load())
                    m_app->close();
                if (m_serverThread.joinable())
                    m_serverThread.join();
                m_running.store(false);
            }

            // Handlers may be on the stack until the server thread joins, so
            // the pins stay until after that join. The caller of stop() still
            // owns this App, so dropping the closures here does not delete it.
            if (m_pin_key && Py_IsInitialized())
                mbpy::clear_retained(m_pin_key);
        }

        [[nodiscard]] mb::MantisBase &app() const { return *m_app; }

    private:
        std::unique_ptr<mb::MantisBase> m_app;
        const void *m_pin_key = nullptr;
        std::thread m_serverThread;
        std::atomic<bool> m_running{false};
    };

    int app_traverse(PyObject *self, visitproc visit, void *arg) {
        if (const PyApp *app = nb::inst_ptr<PyApp>(self); app && app->pin_key()) {
            if (const int rc = mbpy::visit_retained(app->pin_key(), visit, arg))
                return rc;
        }
        Py_VISIT(Py_TYPE(self));
        return 0;
    }

    int app_clear(PyObject *self) {
        if (const PyApp *app = nb::inst_ptr<PyApp>(self); app && app->pin_key())
            mbpy::clear_retained(app->pin_key());
        return 0;
    }

    PyType_Slot app_slots[] = {
        {Py_tp_traverse, reinterpret_cast<void *>(app_traverse)},
        {Py_tp_clear, reinterpret_cast<void *>(app_clear)},
        {0, nullptr},
    };
} // namespace

void register_app(nb::module_ &m) {
    nb::class_<PyApp>(m, "App", "Embedded MantisBase server",
                      nb::type_slots(app_slots))
            .def(nb::init<nb::kwargs>())
            .def("start", &PyApp::start, nb::arg("blocking") = true)
            .def("stop", &PyApp::stop)
            .def("close", &PyApp::stop)
            .def(
                "quit",
                [](PyApp &self, int code, const std::string &reason) {
                    return self.app().quit(code, reason);
                },
                nb::arg("code") = 0, nb::arg("reason") = "Something went wrong!",
                "Stop the server and terminate the process with std::exit.")
            .def("open_admin_dashboard",
                 [](PyApp &self) { self.app().openBrowserOnStart(); })
            .def("snowflake_id", [](PyApp &self) { return self.app().snowflakeId(); })
            .def_prop_rw(
                "host", [](const PyApp &self) { return self.app().host(); },
                [](PyApp &self, const std::string &value) {
                    self.app().setHost(value);
                })
            .def_prop_rw(
                "port", [](const PyApp &self) { return self.app().port(); },
                [](PyApp &self, int value) { self.app().setPort(value); })
            .def_prop_rw(
                "pool_size", [](const PyApp &self) { return self.app().poolSize(); },
                [](PyApp &self, int value) { self.app().setPoolSize(value); })
            .def_prop_rw(
                "public_dir",
                [](const PyApp &self) { return self.app().publicDir(); },
                [](PyApp &self, const std::string &value) {
                    self.app().setPublicDir(value);
                })
            .def_prop_rw(
                "data_dir", [](const PyApp &self) { return self.app().dataDir(); },
                [](PyApp &self, const std::string &value) {
                    self.app().setDataDir(value);
                })
            .def_prop_rw(
                "scripts_dir",
                [](const PyApp &self) { return self.app().scriptsDir(); },
                [](PyApp &self, const std::string &value) {
                    self.app().setScriptsDir(value);
                })
            .def_prop_rw(
                "templates_dir",
                [](const PyApp &self) { return self.app().templatesDir(); },
                [](PyApp &self, const std::string &value) {
                    self.app().setTemplatesDir(value);
                })
            .def_prop_rw(
                "migrations_dir",
                [](const PyApp &self) { return self.app().migrationsDir(); },
                [](PyApp &self, const std::string &value) {
                    self.app().setMigrationsDir(value);
                })
            .def_prop_rw(
                "skip_admin_setup",
                [](const PyApp &self) { return self.app().skipAdminSetup(); },
                [](PyApp &self, bool value) { self.app().setSkipAdminSetup(value); })
            .def_prop_ro("db_type",
                         [](const PyApp &self) { return self.app().dbType(); })
            .def_prop_ro("dev_mode",
                         [](const PyApp &self) { return self.app().isDevMode(); })
            .def_prop_ro(
                "scripting_disabled",
                [](const PyApp &self) { return self.app().isScriptingDisabled(); })
            .def_prop_ro("secret_key",
                         [](const PyApp &self) { return self.app().jwtSecretKey(); })
            .def_prop_ro("version",
                         [](const PyApp &) { return mb::MantisBase::appVersion(); })
            .def_prop_ro("uptime_seconds",
                         [](const PyApp &self) {
                             return std::chrono::duration_cast<std::chrono::seconds>(
                                         std::chrono::steady_clock::now() -
                                         self.app().startTime())
                                     .count();
                         })
            .def_prop_ro(
                "router",
                [](PyApp &self) -> mb::MbRouter & { return self.app().router(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "db", [](PyApp &self) -> mb::MbDatabase & { return self.app().db(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "settings",
                [](PyApp &self) -> mb::MbKVStore & { return self.app().settings(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "auth", [](PyApp &self) -> mb::MbAuth & { return self.app().auth(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "files",
                [](PyApp &self) -> mb::MbFilesMgr & { return self.app().files(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "logs",
                [](PyApp &self) -> mb::MbLogger & { return self.app().logs(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "rt",
                [](PyApp &self) -> mb::MbRealtimeDB & { return self.app().rt(); },
                nb::rv_policy::reference_internal)
            .def_prop_ro(
                "mailer",
                [](PyApp &self) -> mb::MbMailer & { return self.app().mailer(); },
                nb::rv_policy::reference_internal)
            .def(
                "entity",
                [](PyApp &self, const std::string &name) {
                    return self.app().entity(name);
                },
                nb::arg("name"), nb::keep_alive<0, 1>())
            .def(
                "has_entity",
                [](PyApp &self, const std::string &name) {
                    return self.app().hasEntity(name);
                },
                nb::arg("name"))
            .def(
                "create_schema",
                [](PyApp &self, nb::handle schema) {
                    const json body = mbpy::python_to_json(schema);
                    return mbpy::call_json([&] {
                        const mb::MbEntitySchema table =
                                mb::MbEntitySchema::fromSchema(self.app(), body);
                        return mb::MbEntitySchema::createTable(table);
                    });
                },
                nb::arg("schema"))
            .def(
                "list_schemas",
                [](PyApp &self, nb::handle opts) {
                    const json options = mbpy::object_or_empty(opts);
                    return mbpy::call_json([&] {
                        return mb::MbEntitySchema::listTables(self.app(), options);
                    });
                },
                nb::arg("opts") = nb::none())
            .def(
                "get_schema",
                [](PyApp &self, const std::string &table_id) {
                    return mbpy::call_json([&] {
                        return mb::MbEntitySchema::getTable(self.app(), table_id);
                    });
                },
                nb::arg("table_id"))
            .def(
                "update_schema",
                [](PyApp &self, const std::string &table_id, nb::handle schema) {
                    const json body = mbpy::python_to_json(schema);
                    return mbpy::call_json([&] {
                        return mb::MbEntitySchema::updateTable(self.app(), table_id,
                                                               body);
                    });
                },
                nb::arg("table_id"), nb::arg("schema"))
            .def(
                "drop_schema",
                [](PyApp &self, const std::string &table_id) {
                    nb::gil_scoped_release release;
                    mb::MbEntitySchema::dropTable(self.app(), table_id);
                },
                nb::arg("table_id"))
            .def(
                "table_exists",
                [](PyApp &self, const std::string &name) {
                    nb::gil_scoped_release release;
                    return mb::MbEntitySchema::tableExists(self.app(), name);
                },
                nb::arg("name"))
            .def(
                "validate_schema",
                [](PyApp &self, nb::handle schema) -> nb::object {
                    const json body = mbpy::python_to_json(schema);
                    std::optional<std::string> error;
                    {
                        nb::gil_scoped_release release;
                        try {
                            const mb::MbEntitySchema table =
                                    mb::MbEntitySchema::fromSchema(self.app(), body);
                            error = table.validate();
                        } catch (const mb::MbException &ex) {
                            // fromSchema throws on a missing name or type.
                            // validate_schema reports that as a string.
                            error = ex.what();
                        }
                    }
                    if (!error.has_value())
                        return nb::none();
                    return nb::cast(*error);
                },
                nb::arg("schema"));
}
