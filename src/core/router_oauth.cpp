#include "mantisbase/core/router.h"
#include "mantisbase/core/oauth.h"
#include "mantisbase/core/http.h"
#include "mantisbase/mantisbase.h"

namespace mb {
    void Router::registerOAuthRoutes() {
        const Middlewares authEntityMiddleware = {resolveAuthEntity()};

        // User-facing OAuth routes
        Get("/api/v1/auth/:entity_name/oauth/authorize/:provider",
            [](const MantisRequest &req, const MantisResponse &res) {
                try {
                    const auto entity_name = trim(req.getPathParamValue("entity_name"));
                    const auto provider = trim(req.getPathParamValue("provider"));
                    auto redirect_uri = req.getQueryParamValue("redirect_uri");

                    if (redirect_uri.empty()) {
                        const auto host_header = req.getHeaderValue("Host");
                        redirect_uri = std::format("http://{}/api/v1/auth/{}/oauth/callback/",
                                                   host_header, entity_name, provider);
                    }

                    auto result = req.mbApp().auth().oauth().buildAuthorizeUrl(entity_name, provider, redirect_uri);
                    res.setRedirect(result["authorize_url"].get<std::string>());
                } catch (const std::exception &e) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
                }
            }, authEntityMiddleware);

        Get("/api/v1/auth/:entity_name/oauth/callback/:provider",
            [](const MantisRequest &req, const MantisResponse &res) {
                try {
                    const auto entity_name = trim(req.getPathParamValue("entity_name"));
                    const auto provider = trim(req.getPathParamValue("provider"));
                    const auto code = req.getQueryParamValue("code");
                    const auto state = req.getQueryParamValue("state");

                    if (code.empty() || state.empty()) {
                        auto error = req.getQueryParamValue("error");
                        res.sendJSON(400, {
                                         {"status", 400},
                                         {"data", json::object()},
                                         {"error", error.empty() ? "Missing code or state parameter" : error}
                                     });
                        return;
                    }

                    auto result = req.mbApp().auth().oauth().handleCallback(entity_name, provider, code, state);
                    res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
                } catch (const std::exception &e) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
                }
            }, authEntityMiddleware);

        Post("/api/v1/auth/:entity_name/oauth/link/:provider",
            [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(401, {
                                     {"status", 401}, {"data", json::object()}, {"error", "Authentication required"}
                                 });
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

                const auto code = body.value("code", "");
                const auto state = body.value("state", "");

                auto result = req.mbApp().auth().oauth().linkAccount(entity_name, user_id, provider, code, state);
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Delete("/api/v1/auth/:entity_name/oauth/link/:provider",
               [](MantisRequest &req, const MantisResponse &res) {
                   try {
                       auto auth = req.getOr<json>("auth", json::object());
                       auto verification = req.getOr<json>("verification", json::object());

                       if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                           res.sendJSON(401, {
                                            {"status", 401}, {"data", json::object()},
                                            {"error", "Authentication required"}
                                        });
                           return;
                       }

                       auto entity_name = trim(req.getPathParamValue("entity_name"));
                       auto provider = trim(req.getPathParamValue("provider"));
                       auto user_id = auth["id"].get<std::string>();

                       if (req.mbApp().auth().oauth().unlinkAccount(entity_name, user_id, provider)) {
                           res.sendJSON(200, {{"status", 200}, {"data", {{"unlinked", true}}}, {"error", ""}});
                       } else {
                           res.sendJSON(404, {
                                            {"status", 404}, {"data", json::object()},
                                            {"error", "Linked account not found"}
                                        });
                       }
                   } catch (const std::exception &e) {
                       res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
                   }
               }, authEntityMiddleware);

        Get("/api/v1/auth/:entity_name/oauth/accounts",
            [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(401, {
                                     {"status", 401}, {"data", json::object()}, {"error", "Authentication required"}
                                 });
                    return;
                }

                auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto user_id = auth["id"].get<std::string>();

                auto accounts = req.mbApp().auth().oauth().getLinkedAccounts(entity_name, user_id);
                res.sendJSON(200, {{"status", 200}, {"data", accounts}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        Get("/api/v1/auth/:entity_name/oauth/providers",
            [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto entity_name = trim(req.getPathParamValue("entity_name"));
                auto providers = req.mbApp().auth().oauth().getProviders(entity_name);
                res.sendJSON(200, {{"status", 200}, {"data", providers}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, authEntityMiddleware);

        // Admin OAuth provider management routes
        const Middlewares adminAuth = {requireAdminAuth()};

        Post("/api/v1/sys/oauth/providers",
            [](MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                for (const auto &key: {"name", "client_id", "client_secret"}) {
                    if (!body.contains(key)) {
                        res.sendJSON(400, {
                                         {"status", 400}, {"data", json::object()},
                                         {"error", std::string("Missing required field: ") + key}
                                     });
                        return;
                    }
                }

                auto result = req.mbApp().auth().oauth().addProvider(body);
                res.sendJSON(201, {{"status", 201}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Get("/api/v1/sys/oauth/providers",
            [](const MantisRequest &req, const MantisResponse &res) {
            try {
                auto providers = req.mbApp().auth().oauth().listProviders();
                res.sendJSON(200, {{"status", 200}, {"data", providers}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Patch("/api/v1/sys/oauth/providers/:id",
            [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto provider_id = trim(req.getPathParamValue("id"));
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                auto result = req.mbApp().auth().oauth().updateProvider(provider_id, body);
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(500, {{"status", 500}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Delete("/api/v1/sys/oauth/providers/:id",
            [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto provider_id = trim(req.getPathParamValue("id"));
                if (req.mbApp().auth().oauth().removeProvider(provider_id)) {
                    res.sendJSON(200, {{"status", 200}, {"data", {{"deleted", true}}}, {"error", ""}});
                } else {
                    res.sendJSON(404, {{"status", 404}, {"data", json::object()}, {"error", "Provider not found"}});
                }
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Post("/api/v1/sys/oauth/entity-config",
            [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                auto entity_name = body.at("entity_name").get<std::string>();
                auto provider_id = body.at("provider_id").get<std::string>();

                auto result = req.mbApp().auth().oauth().enableProviderForEntity(entity_name, provider_id);
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const std::exception &e) {
                res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", e.what()}});
            }
        }, adminAuth);

        Delete("/api/v1/sys/oauth/entity-config",
            [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }

                const auto entity_name = body.at("entity_name").get<std::string>();
                const auto provider_id = body.at("provider_id").get<std::string>();

                if (req.mbApp().auth().oauth().disableProviderForEntity(entity_name, provider_id)) {
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
