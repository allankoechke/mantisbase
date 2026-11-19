#include <mantisbase/core/email.h>
#include <mantisbase/core/files.h>
#include <mantisbase/core/kv_store.h>
#include <mantisbase/core/logger/logger.h>
#include <mantisbase/core/realtime.h>
#include <mantisbase/core/sse.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <unordered_map>

#include "py_conv.h"

namespace nb = nanobind;

namespace {
    nlohmann::json log_data(nb::handle data) {
        if (!data.is_valid() || data.is_none())
            return nlohmann::json::object();
        return mbpy::python_to_json(data);
    }

    std::vector<std::string> string_list(nb::handle values, const char *message) {
        nlohmann::json listed = mbpy::python_to_json(values);
        if (!listed.is_array())
            throw nb::type_error(message);
        std::vector<std::string> out;
        out.reserve(listed.size());
        for (const auto &item: listed) {
            if (!item.is_string())
                throw nb::type_error(message);
            out.push_back(item.get<std::string>());
        }
        return out;
    }

    void bind_log(nb::class_<mb::MbLogger> &cls, const char *name,
                  void (mb::MbLogger::*method)(const std::string &,
                                               const std::string &,
                                               const std::string &,
                                               const nlohmann::json &) const) {
        cls.def(
            name,
            [method](const mb::MbLogger &self, const std::string &origin,
                     const std::string &message, const std::string &details,
                     nb::handle data) {
                const nlohmann::json payload = log_data(data);
                nb::gil_scoped_release release;
                (self.*method)(origin, message, details, payload);
            },
            nb::arg("origin"), nb::arg("message"), nb::arg("details") = "",
            nb::arg("data") = nb::none());
    }
} // namespace

void register_services(nb::module_ &m) {
    nb::class_<mb::MbFilesMgr>(m, "Files", "Entity file storage")
            .def(
                "create_dir",
                [](const mb::MbFilesMgr &self, const std::string &entity) {
                    nb::gil_scoped_release release;
                    self.createDir(entity);
                },
                nb::arg("entity"))
            .def(
                "rename_dir",
                [](const mb::MbFilesMgr &self, const std::string &from,
                   const std::string &to) {
                    nb::gil_scoped_release release;
                    self.renameDir(from, to);
                },
                nb::arg("old"), nb::arg("new"))
            .def(
                "delete_dir",
                [](const mb::MbFilesMgr &self, const std::string &entity) {
                    nb::gil_scoped_release release;
                    self.deleteDir(entity);
                },
                nb::arg("entity"))
            .def(
                "dir_path",
                [](const mb::MbFilesMgr &self, const std::string &entity,
                   bool create_if_missing) {
                    nb::gil_scoped_release release;
                    return self.dirPath(entity, create_if_missing);
                },
                nb::arg("entity"), nb::arg("create_if_missing") = false)
            .def(
                "file_path",
                [](const mb::MbFilesMgr &self, const std::string &entity,
                   const std::string &filename) {
                    nb::gil_scoped_release release;
                    return self.filePath(entity, filename);
                },
                nb::arg("entity"), nb::arg("filename"))
            .def(
                "get_file_path",
                [](mb::MbFilesMgr &self, const std::string &entity,
                   const std::string &filename) -> nb::object {
                    std::optional<std::string> path;
                    {
                        nb::gil_scoped_release release;
                        path = self.getFilePath(entity, filename);
                    }
                    if (!path.has_value())
                        return nb::none();
                    return nb::cast(*path);
                },
                nb::arg("entity"), nb::arg("filename"))
            .def(
                "remove_file",
                [](const mb::MbFilesMgr &self, const std::string &entity,
                   const std::string &filename) {
                    nb::gil_scoped_release release;
                    return self.removeFile(entity, filename);
                },
                nb::arg("entity"), nb::arg("filename"))
            .def(
                "remove_files",
                [](const mb::MbFilesMgr &self, const std::string &entity,
                   nb::handle files) {
                    const std::vector<std::string> names =
                            string_list(files, "files must be a list of filenames");
                    nb::gil_scoped_release release;
                    self.removeFiles(entity, names);
                },
                nb::arg("entity"), nb::arg("files"))
            .def(
                "file_exists",
                [](const mb::MbFilesMgr &self, const std::string &entity,
                   const std::string &filename) {
                    nb::gil_scoped_release release;
                    return self.fileExists(entity, filename);
                },
                nb::arg("entity"), nb::arg("filename"))
            .def(
                "canonical_path",
                [](const mb::MbFilesMgr &self, const std::string &path) {
                    return self.getCanonicalPath(path).string();
                },
                nb::arg("path"))
            .def(
                "is_canonical",
                [](const mb::MbFilesMgr &self, const std::string &path) {
                    return self.isCanonicalPath(path);
                },
                nb::arg("path"))
            .def_prop_ro("base_dir", [](const mb::MbFilesMgr &self) {
                return self.filesBaseDir().string();
            });

    nb::class_<mb::MbKVStore>(m, "Settings",
                              "Application settings stored in the database")
            .def(
                "get",
                [](const mb::MbKVStore &self, const std::string &key) {
                    return mbpy::call_json([&] { return self.getScriptingValue(key); });
                },
                nb::arg("key"))
            .def(
                "set",
                [](mb::MbKVStore &self, const std::string &key, nb::handle value) {
                    const nlohmann::json body = mbpy::python_to_json(value);
                    nb::gil_scoped_release release;
                    self.setScriptingValue(key, body);
                },
                nb::arg("key"), nb::arg("value"))
            .def("configs",
                 [](const mb::MbKVStore &self) {
                     return mbpy::call_json(
                         [&] { return self.getScriptingConfigsCopy(); });
                 })
            .def("reload", [](mb::MbKVStore &self) {
                nb::gil_scoped_release release;
                self.reloadScriptingConfigs();
            });

    auto logs = nb::class_<mb::MbLogger>(m, "Logs", "Structured application log");
    bind_log(logs, "trace", &mb::MbLogger::trace);
    bind_log(logs, "debug", &mb::MbLogger::debug);
    bind_log(logs, "info", &mb::MbLogger::info);
    bind_log(logs, "warn", &mb::MbLogger::warn);
    bind_log(logs, "critical", &mb::MbLogger::critical);
    bind_log(logs, "error", &mb::MbLogger::critical);

    nb::class_<mb::MbRealtimeDB>(m, "Realtime", "Database change notifications")
            .def("notify_change",
                 [](const mb::MbRealtimeDB &self) {
                     nb::gil_scoped_release release;
                     self.notifyChange();
                 })
            .def(
                "add_hooks",
                [](const mb::MbRealtimeDB &self, const std::string &entity) {
                    nb::gil_scoped_release release;
                    self.addDbHooks(entity);
                },
                nb::arg("entity"))
            .def(
                "drop_hooks",
                [](const mb::MbRealtimeDB &self, const std::string &entity) {
                    nb::gil_scoped_release release;
                    self.dropDbHooks(entity);
                },
                nb::arg("entity"))
            .def(
                "broadcast",
                [](const mb::MbRealtimeDB &self, nb::handle event) {
                    const nlohmann::json body = mbpy::python_to_json(event);
                    nb::gil_scoped_release release;
                    self.mbApp().router().sseMgr().broadcastChange(body);
                },
                nb::arg("event"));

    nb::class_<mb::MbSSEMgr>(m, "Sse", "Server-Sent Events and WebSocket fan-out")
            .def(
                "broadcast",
                [](mb::MbSSEMgr &self, nb::handle event) {
                    const nlohmann::json body = mbpy::python_to_json(event);
                    nb::gil_scoped_release release;
                    self.broadcastChange(body);
                },
                nb::arg("event"))
            .def("session_count", &mb::MbSSEMgr::getSessionCount)
            .def("remove_session", &mb::MbSSEMgr::removeSession, nb::arg("client_id"))
            .def("is_running", &mb::MbSSEMgr::isRunning)
            .def("ws_connection_count",
                 [](mb::MbSSEMgr &self) { return self.wsMgr().connectionCount(); });

    nb::class_<mb::MbMailer>(m, "Mailer", "Outbound SMTP mail")
            .def_prop_ro("is_configured", &mb::MbMailer::isConfigured)
            .def_prop_ro(
                "config",
                [](const mb::MbMailer &self) {
                    const mb::MbSmtpConfig &config = self.config();
                    return mbpy::json_to_python({
                        {"host", config.host},
                        {"port", config.port},
                        {"user", config.user},
                        {"password", config.password.empty() ? "" : "********"},
                        {"from", config.from_address},
                        {"tls", config.use_tls},
                    });
                })
            .def(
                "configure",
                [](mb::MbMailer &self, nb::handle config) {
                    const nlohmann::json body = mbpy::python_to_json(config);
                    self.configure(mb::MbSmtpConfig::fromJson(body));
                },
                nb::arg("smtp"),
                "Apply an SMTP dict: host, port, user, password, from, tls.")
            .def(
                "apply_settings",
                [](mb::MbMailer &self, nb::handle configs) {
                    const nlohmann::json body = mbpy::python_to_json(configs);
                    self.applySettings(body);
                },
                nb::arg("configs"), "Read the smtp object from a settings dict.")
            .def(
                "send",
                [](mb::MbMailer &self, const std::string &to,
                   const std::string &subject, const std::string &html,
                   const std::string &text) {
                    mb::MbEmailMsg message;
                    message.to = to;
                    message.subject = subject;
                    message.body_html = html;
                    message.body_text = text;
                    nb::gil_scoped_release release;
                    return self.send(message);
                },
                nb::arg("to"), nb::arg("subject"), nb::arg("html") = "",
                nb::arg("text") = "")
            .def(
                "send_test",
                [](mb::MbMailer &self, const std::string &to) {
                    nb::gil_scoped_release release;
                    return self.sendTest(to);
                },
                nb::arg("to"))
            .def(
                "load_template",
                [](const mb::MbMailer &self, const std::string &name,
                   nb::handle variables) {
                    std::unordered_map<std::string, std::string> values;
                    const nlohmann::json object = mbpy::object_or_empty(variables);
                    for (auto it = object.begin(); it != object.end(); ++it) {
                        if (!it.value().is_string())
                            throw nb::type_error("template variables must be strings");
                        values.emplace(it.key(), it.value().get<std::string>());
                    }
                    return self.loadTemplate(name, values);
                },
                nb::arg("name"), nb::arg("variables") = nb::none())
            .def("sent_messages",
                 [](const mb::MbMailer &self) {
                     nb::list out;
                     for (const mb::MbEmailMsg &message: self.sentMessages()) {
                         nb::dict item;
                         item["to"] = message.to;
                         item["subject"] = message.subject;
                         item["html"] = message.body_html;
                         item["text"] = message.body_text;
                         out.append(std::move(item));
                     }
                     return out;
                 })
            .def("clear_sent_messages", &mb::MbMailer::clearSentMessages)
            .def("clear_rate_limits", &mb::MbMailer::clearRateLimits)
            .def("check_rate_limit", &mb::MbMailer::checkRateLimit, nb::arg("email"),
                 nb::arg("max_per_hour") = 3);
}
