#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <mantisbase/mantisbase.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdlib>

namespace nb = nanobind;
using json = nlohmann::json;

class PyApp {
public:
    PyApp(nb::kwargs kwargs) {
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
            config["migrations-dir"] = nb::cast<std::string>(kwargs["migrations_dir"]);
        if (kwargs.contains("pool_size"))
            config["serve"]["pool-size"] = nb::cast<int>(kwargs["pool_size"]);
        // `dev` is a presence-only flag in MantisBase::create() -- only add the
        // key when the caller actually asked for dev mode.
        if (kwargs.contains("dev") && nb::cast<bool>(kwargs["dev"]))
            config["dev"] = true;
        if (kwargs.contains("skip_admin_setup"))
            config["serve"]["skip-admin-setup"] = nb::cast<bool>(kwargs["skip_admin_setup"]);

        // MantisBase reads the JWT signing key from the environment, so there is
        // no CLI/config equivalent to forward it through.
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
    }

    ~PyApp() {
        if (m_running.load()) {
            m_app->close();
            if (m_serverThread.joinable())
                m_serverThread.join();
        }
    }

    void start(bool blocking) {
        if (m_running.load())
            return;

        m_running.store(true);

        if (blocking) {
            nb::gil_scoped_release release;
            m_app->run();
            m_running.store(false);
        } else {
            m_serverThread = std::thread([this]() {
                m_app->run();
                m_running.store(false);
            });
        }
    }

    void stop() {
        if (!m_running.load())
            return;

        m_app->close();
        if (m_serverThread.joinable())
            m_serverThread.join();
        m_running.store(false);
    }

    mb::Router& router() { return m_app->router(); }
    mb::Database& db() { return m_app->db(); }
    mb::MantisBase& app() { return *m_app; }

private:
    std::unique_ptr<mb::MantisBase> m_app;
    std::thread m_serverThread;
    std::atomic<bool> m_running{false};
};

void register_app(nb::module_& m) {
    nb::class_<PyApp>(m, "App")
        .def(nb::init<nb::kwargs>())
        .def("start", &PyApp::start, nb::arg("blocking") = true)
        .def("stop", &PyApp::stop)
        .def_prop_ro("router", &PyApp::router, nb::rv_policy::reference_internal)
        .def_prop_ro("db", &PyApp::db, nb::rv_policy::reference_internal);
}
