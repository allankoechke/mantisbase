#include "../../include/mantisbase/core/router.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/models/validators.h"

namespace mb {
    std::function<void(MantisRequest &, MantisResponse &)> Router::handleAuthLogin() {
        return [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(500, {
                        {"status", 500},
                        {"data", json::object()},
                        {"error", err}
                    });
                    return;
                }

                for (const auto &key: std::vector<std::string>{"identity", "password"}) {
                    if (!body.contains(key) || !body[key].is_string() || body[key].empty()) {
                        res.sendJSON(400, {
                            {"status", 400},
                            {"data", json::object()},
                            {"error", "Expected `" + key + "` key in the request body."}
                        });
                        return;
                    }
                }

                const auto entity_name = trim(req.getPathParamValue("entity_name"));
                const auto entity = MantisBase::instance().entity(entity_name);

                auto opt_user = entity.queryFromCols(body["identity"].get<std::string>(), {"id", "email"});
                if (!opt_user.has_value()) {
                    res.sendJSON(404, {
                        {"status", 404},
                        {"data", json::object()},
                        {"error", "No user found for given `identity`, `password` & `entity` combination."}
                    });
                    return;
                }

                auto &user = opt_user.value();

                // OAuth-only users have no password
                if (user["password"].is_null()) {
                    res.sendJSON(400, {
                        {"status", 400},
                        {"data", json::object()},
                        {"error", "This account uses OAuth login. Please sign in with your linked provider."}
                    });
                    return;
                }

                if (!verifyPassword(body["password"].get<std::string>(), user["password"].get<std::string>())) {
                    res.sendJSON(404, {
                        {"status", 404},
                        {"data", json::object()},
                        {"error", "No user found matching given `identity`, `password` and `entity` combination."}
                    });
                    auto _body = body;
                    _body.erase("password");
                    LogOrigin::authWarn("User Not Found", fmt::format("No user found matching given data: \n\t- {}", _body.dump()));
                    return;
                }

                auto token = Auth::createToken({{"id", user["id"]}, {"entity", entity.name()}});

                user.erase("password");
                res.sendJSON(200, {
                    {"status", 200},
                    {"data", {{"token", token}, {"user", user}}},
                    {"error", ""}
                });
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"status", e.code()},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"status", 500},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            }
        };
    }

    std::function<void(MantisRequest &, MantisResponse &)> Router::handleAdminLogin() {
         return [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(500, {
                        {"status", 500},
                        {"data", json::object()},
                        {"error", err}
                    });
                    return;
                }

                for (const auto &key: std::vector<std::string>{"identity", "password"}) {
                    if (!body.contains(key) || !body[key].is_string() || body[key].empty()) {
                        res.sendJSON(400, {
                            {"status", 400},
                            {"data", json::object()},
                            {"error", "Expected `" + key + "` key in the request body."}
                        });
                        return;
                    }
                }

                const auto entity = MantisBase::instance().entity("mb_admins");

                auto opt_user = entity.queryFromCols(
                    body["identity"].get<std::string>(),
                    {"id", "email"}
                );

                if (!opt_user.has_value()) {
                    res.sendJSON(404, {
                        {"status", 404},
                        {"data", json::object()},
                        {"error", "No user found for given `identity` and `password` combination."}
                    });
                    return;
                }

                auto &user = opt_user.value();

                if (!verifyPassword(body["password"].get<std::string>(), user["password"].get<std::string>())) {
                    res.sendJSON(404, {
                        {"status", 404},
                        {"data", json::object()},
                        {"error", "No user found matching given `identity`, `password` and `entity` combination."}
                    });
                    auto _body = body;
                    _body.erase("password");
                    LogOrigin::authWarn("Admin User Not Found", fmt::format("No user found matching given data: \n\t- {}", _body.dump()));
                    return;
                }

                auto token = Auth::createToken({{"id", user["id"]}, {"entity", entity.name()}});

                user.erase("password");
                res.sendJSON(200, {
                    {"status", 200},
                    {"data", {{"token", token}, {"user", user}}},
                    {"error", ""}
                });
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"status", e.code()},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"status", 500},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            }
        };
    }

    std::function<void(MantisRequest &, MantisResponse &)> Router::handleAuthRefresh() {
        return [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {
                        {"status", 403},
                        {"data", json::object()},
                        {"error", "Valid token required to refresh"}
                    });
                    return;
                }

                auto claims = verification["claims"];
                auto session_id = claims.value("session_id", "");
                auto entity_name = claims["entity"].get<std::string>();
                auto user_id = claims["id"].get<std::string>();

                if (session_id.empty()) {
                    res.sendJSON(400, {
                        {"status", 400},
                        {"data", json::object()},
                        {"error", "Token does not contain a session"}
                    });
                    return;
                }

                auto result = Auth::refreshSession(session_id, entity_name, user_id);

                // Get user record
                const auto entity = MantisBase::instance().entity(entity_name);
                auto user_opt = entity.read(user_id);
                json user = user_opt.has_value() ? user_opt.value() : json::object();
                user.erase("password");

                res.sendJSON(200, {
                    {"status", 200},
                    {"data", {{"token", result["token"]}, {"user", user}}},
                    {"error", ""}
                });
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"status", e.code()},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"status", 500},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            }
        };
    }

    std::function<void(MantisRequest &, MantisResponse &)> Router::handleAuthLogout() {
        return [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(403, {
                        {"status", 403},
                        {"data", json::object()},
                        {"error", "Valid token required to logout"}
                    });
                    return;
                }

                auto claims = verification["claims"];
                auto session_id = claims.value("session_id", "");

                if (!session_id.empty()) {
                    Auth::deleteSession(session_id);
                }

                res.sendJSON(200, {
                    {"status", 200},
                    {"data", {{"logged_out", true}}},
                    {"error", ""}
                });
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"status", e.code()},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"status", 500},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            }
        };
    }

    std::function<void(MantisRequest &, MantisResponse &)> Router::handleSetupAdmin() {
        return [](MantisRequest &req, const MantisResponse &res) {
            TRACE_MB_FUNC();
            try {
                auto auth = req.getOr("auth", json::object());
                LogOrigin::authTrace("Auth Data", fmt::format("Auth Data: {}", auth.dump()));

                auto verification = req.getOr<json>("verification", json::object());
                if (verification.empty()) {
                    res.sendJSON(403, {
                        {"data", json::object()},
                        {"status", 403},
                        {"error", "Auth required to access this resource!"}
                    });
                    return;
                }

                const bool verified = verification.contains("verified") &&
                                      verification["verified"].is_boolean() &&
                                      verification["verified"].get<bool>();

                if (!verified) {
                    res.sendJSON(403, {
                        {"data", json::object()},
                        {"status", 403},
                        {"error", verification["error"]}
                    });
                    return;
                }

                if (!auth["entity"].is_string() || auth["entity"].get<std::string>() != "mb_service_acc") {
                    res.sendJSON(403, {
                        {"data", json::object()},
                        {"status", 403},
                        {"error", "Expected a service account token, access denied!"}
                    });
                    return;
                }

                if (auth["user"].is_null()) {
                    res.sendJSON(404, {
                        {"data", json::object()},
                        {"status", 403},
                        {"error", "Auth service account does not exist!"}
                    });
                    return;
                }

                const auto entity = MantisBase::instance().entity("mb_service_acc");
                const auto admin_entity = MantisBase::instance().entity("mb_admins");

                const auto &[body, err] = req.getBodyAsJson();

                if (!err.empty()) {
                    res.sendJSON(400, {
                        {"data", json::object()},
                        {"status", 400},
                        {"error", err}
                    });
                    return;
                }

                if (const auto v_err = Validators::validateRequestBody(admin_entity.schema(), body);
                    v_err.has_value()) {
                    LogOrigin::critical("Request Validation Error", fmt::format("Error validating request body\n\t- {}", v_err.value()));
                    res.sendJSON(400, {
                        {"data", json::object()},
                        {"status", 400},
                        {"error", v_err.value()}
                    });
                    return;
                }

                const auto admin_user = admin_entity.create(body);
                res.sendJSON(201, admin_user);

                entity.remove(auth["id"].get<std::string>());
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"status", e.code()},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"status", 500},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            }
        };
    }

}
