#include "mantisbase/core/router.h"
#include "mantisbase/core/api_keys.h"
#include "mantisbase/core/http.h"
#include "mantisbase/mantisbase.h"

namespace mb {
    void Router::registerApiKeyRoutes() {
        const Middlewares authEntityMiddleware = {resolveAuthEntity()};

        // Entity-scoped API key routes
        Post("/api/v1/auth/:entity_name/api-keys", [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {{"status", 403}, {"data", json::object()}, {"error", "Authentication required"}});
                    return;
                }

                auto user_id = auth["id"].get<std::string>();
                auto entity_name = trim(req.getPathParamValue("entity_name"));

                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                const auto label = body.value("label", "API Key");
                const auto permissions = body.value("permissions", json::array());
                const auto expires_at = body.value("expires_at", "");

                auto result = req.mbApp().auth().apiKey().create(entity_name, user_id, label, permissions, expires_at);
                res.sendJSON(201, {{"status", 201}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Get("/api/v1/auth/:entity_name/api-keys", [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto& auth = req.getOr<json>("auth", json::object());
                auto& verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {{"status", 403}, {"data", json::object()}, {"error", "Authentication required"}});
                    return;
                }

                const auto user_id = auth["id"].get<std::string>();
                const auto entity_name = trim(req.getPathParamValue("entity_name"));

                auto keys = req.mbApp().auth().apiKey().list(entity_name, user_id);
                res.sendJSON(200, {{"status", 200}, {"data", keys}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Delete("/api/v1/auth/:entity_name/api-keys/:id", [this](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {{"status", 403}, {"data", json::object()}, {"error", "Authentication required"}});
                    return;
                }

                const auto user_id = auth["id"].get<std::string>();
                const auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto key_id = trim(req.getPathParamValue("id"));

                if (req.mbApp().auth().apiKey().revoke(key_id, entity_name, user_id)) {
                    res.sendJSON(200, {{"status", 200}, {"data", {{"deleted", true}}}, {"error", ""}});
                } else {
                    res.sendJSON(404, {{"status", 404}, {"data", json::object()}, {"error", "API key not found"}});
                }
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        // System-scoped API key routes (admin)
        const Middlewares adminAuth = {requireAdminAuth()};

        Post("/api/v1/sys/api-keys", [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto user_id = auth["id"].get<std::string>();

                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                const auto label = body.value("label", "Admin API Key");
                const auto permissions = body.value("permissions", json::array());
                const auto expires_at = body.value("expires_at", "");

                auto result = req.mbApp().auth().apiKey().createAdmin(user_id, label, permissions, expires_at);
                res.sendJSON(201, {{"status", 201}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Get("/api/v1/sys/api-keys", [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto keys = req.mbApp().auth().apiKey().listAdmin();
                res.sendJSON(200, {{"status", 200}, {"data", keys}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Delete("/api/v1/sys/api-keys/:id", [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                const auto user_id = auth["id"].get<std::string>();
                auto key_id = trim(req.getPathParamValue("id"));

                if (req.mbApp().auth().apiKey().revokeAdmin(key_id, user_id)) {
                    res.sendJSON(200, {{"status", 200}, {"data", {{"deleted", true}}}, {"error", ""}});
                } else {
                    res.sendJSON(404, {{"status", 404}, {"data", json::object()}, {"error", "API key not found"}});
                }
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);
    }
} // mb
