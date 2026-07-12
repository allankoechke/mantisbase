#include "mantisbase/core/router.h"
#include "mantisbase/core/oauth.h"
#include "mantisbase/core/http.h"
#include "mantisbase/mantisbase.h"

namespace mb {
    void Router::registerOAuthRoutes() {
        const Middlewares authEntityMiddleware = {resolveAuthEntity()};

        // User-facing OAuth routes
        Get("/api/v1/auth/:entity_name/oauth/authorize/:provider", [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto provider = trim(req.getPathParamValue("provider"));
                auto redirect_uri = req.getQueryParamValue("redirect_uri");

                if (redirect_uri.empty()) {
                    auto host_header = req.getHeaderValue("Host");
                    redirect_uri = "http://" + host_header + "/api/v1/auth/" + entity_name + "/oauth/callback/" + provider;
                }

                auto result = OAuthManager::buildAuthorizeUrl(entity_name, provider, redirect_uri);
                res.setRedirect(result["authorize_url"].get<std::string>());
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Get("/api/v1/auth/:entity_name/oauth/callback/:provider", [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto provider = trim(req.getPathParamValue("provider"));
                auto code = req.getQueryParamValue("code");
                auto state = req.getQueryParamValue("state");

                if (code.empty() || state.empty()) {
                    auto error = req.getQueryParamValue("error");
                    res.sendJSON(400, {
                        {"status", 400},
                        {"data", json::object()},
                        {"error", error.empty() ? "Missing code or state parameter" : error}
                    });
                    return;
                }

                auto result = OAuthManager::handleCallback(entity_name, provider, code, state);
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Post("/api/v1/auth/:entity_name/oauth/link/:provider", [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {{"status", 403}, {"data", json::object()}, {"error", "Authentication required"}});
                    return;
                }

                auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto provider = trim(req.getPathParamValue("provider"));
                auto user_id = auth["id"].get<std::string>();

                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                auto code = body.value("code", "");
                auto state = body.value("state", "");

                auto result = OAuthManager::linkAccount(entity_name, user_id, provider, code, state);
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Delete("/api/v1/auth/:entity_name/oauth/link/:provider", [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = json::object(); // req.getOr<json>("auth", json::object());
                auto verification = json::object(); // req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {{"status", 403}, {"data", json::object()}, {"error", "Authentication required"}});
                    return;
                }

                auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto provider = trim(req.getPathParamValue("provider"));
                auto user_id = auth["id"].get<std::string>();

                if (OAuthManager::unlinkAccount(entity_name, user_id, provider)) {
                    res.sendJSON(200, {{"status", 200}, {"data", {{"unlinked", true}}}, {"error", ""}});
                } else {
                    res.sendJSON(404, {{"status", 404}, {"data", json::object()}, {"error", "Linked account not found"}});
                }
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Get("/api/v1/auth/:entity_name/oauth/accounts", [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = json::object(); // req.getOr<json>("auth", json::object());
                auto verification = json::object(); // req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {{"status", 403}, {"data", json::object()}, {"error", "Authentication required"}});
                    return;
                }

                auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto user_id = auth["id"].get<std::string>();

                auto accounts = OAuthManager::getLinkedAccounts(entity_name, user_id);
                res.sendJSON(200, {{"status", 200}, {"data", accounts}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Get("/api/v1/auth/:entity_name/oauth/providers", [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto providers = OAuthManager::getProviders(entity_name);
                res.sendJSON(200, {{"status", 200}, {"data", providers}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        // Admin OAuth provider management routes
        const Middlewares adminAuth = {requireAdminAuth()};

        Post("/api/v1/sys/oauth/providers", [](MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                for (const auto &key : {"name", "client_id", "client_secret"}) {
                    if (!body.contains(key)) {
                        res.sendJSON(400, {
                            {"status", 400}, {"data", json::object()},
                            {"error", std::string("Missing required field: ") + key}
                        });
                        return;
                    }
                }

                auto result = OAuthManager::addProvider(body);
                res.sendJSON(201, {{"status", 201}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Get("/api/v1/sys/oauth/providers", [](const MantisRequest &, const MantisResponse &res) {
            try {
                auto providers = OAuthManager::listProviders();
                res.sendJSON(200, {{"status", 200}, {"data", providers}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Patch("/api/v1/sys/oauth/providers/:id", [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto provider_id = trim(req.getPathParamValue("id"));
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                auto result = OAuthManager::updateProvider(provider_id, body);
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Delete("/api/v1/sys/oauth/providers/:id", [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto provider_id = trim(req.getPathParamValue("id"));
                if (OAuthManager::removeProvider(provider_id)) {
                    res.sendJSON(200, {{"status", 200}, {"data", {{"deleted", true}}}, {"error", ""}});
                } else {
                    res.sendJSON(404, {{"status", 404}, {"data", json::object()}, {"error", "Provider not found"}});
                }
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Post("/api/v1/sys/oauth/entity-config", [](MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                auto entity_name = body.at("entity_name").get<std::string>();
                auto provider_id = body.at("provider_id").get<std::string>();

                auto result = OAuthManager::enableProviderForEntity(entity_name, provider_id);
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Delete("/api/v1/sys/oauth/entity-config", [](MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                auto entity_name = body.at("entity_name").get<std::string>();
                auto provider_id = body.at("provider_id").get<std::string>();

                if (OAuthManager::disableProviderForEntity(entity_name, provider_id)) {
                    res.sendJSON(200, {{"status", 200}, {"data", {{"disabled", true}}}, {"error", ""}});
                } else {
                    res.sendJSON(404, {{"status", 404}, {"data", json::object()}, {"error", "Config not found"}});
                }
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);
    }
} // mb
