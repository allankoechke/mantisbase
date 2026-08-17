#include "../../include/mantisbase/core/router.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/models/validators.h"
#include "../../include/mantisbase/utils/utils.h"
#include "drogon/drogon_callbacks.h"

namespace mb {
    const std::function<drogon::HttpResponsePtr(const drogon::HttpRequestPtr &)> Router::reqIdSyncAdvice() {
        return [this](const drogon::HttpRequestPtr &req) {
            // Generate and store request ID in attributes
            std::string requestId = fmt::format("req_{}", m_sfId.nextID());
            req->attributes()->insert("request_id", requestId);

            // Return nullptr to continue normal processing
            return nullptr;
        };
    }

    std::function<void(const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp)>
    Router::loggerPostHandlingAdvice() const {
        return [this](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
            const auto start = req->creationDate();
            const auto end = trantor::Date::now();
            const auto duration = end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch();
            auto seconds = static_cast<double>(duration) / 1000000.0;

            logger().info(
                "HTTP",
                fmt::format("{} {}{} {} {}s {}B {} {} {}",
                            req->methodString(),
                            req->path(),
                            req->query().empty() ? "" : "?" + req->query(),
                            static_cast<int>(resp->getStatusCode()),
                            seconds,
                            resp->body().length(),
                            req->versionString(),
                            req->peerAddr().toIp(),
                            req->attributes()->get<std::string>("request_id")
                )
            );
        };
    }

    void Router::reloadCorsOrigins() {
        auto origins = std::make_shared<std::set<std::string>>();

        const auto &cfg = mbApp().settings().configs();
        if (cfg.contains("corsAllowedOrigins") && cfg["corsAllowedOrigins"].is_array()) {
            for (const auto &item : cfg["corsAllowedOrigins"]) {
                if (item.is_string()) {
                    const auto value = trim(item.get<std::string>());
                    if (!value.empty()) {
                        origins->insert(value);
                    }
                }
            }
        }

        if (const auto raw = getEnvOrDefault("MB_CORS_ORIGINS", ""); !raw.empty()) {
            for (const auto &part : splitString(raw, ",")) {
                const auto value = trim(part);
                if (!value.empty()) {
                    origins->insert(value);
                }
            }
        }

        m_corsAllowedOrigins.store(origins);

        logger().info("CORS", fmt::format("Loaded {} allowed origin(s)", origins->size()));
    }

    bool Router::isOriginAllowed(const std::string &origin) const {
        const auto allowed = m_corsAllowedOrigins.load();
        return allowed && allowed->count(origin) > 0;
    }

    void Router::applyCorsHeaders(const drogon::HttpRequestPtr &req,
                                  const drogon::HttpResponsePtr &resp) const {
        const auto &origin = req->getHeader("Origin");
        if (origin.empty() || !isOriginAllowed(origin)) {
            return;
        }

        resp->addHeader("Access-Control-Allow-Origin", origin);
        resp->addHeader("Access-Control-Allow-Credentials", "true");
    }

    std::function<void(const drogon::HttpRequestPtr &,
                       drogon::AdviceCallback &&,
                       drogon::AdviceChainCallback &&
    )>
    Router::corsPreRoutingAdvice() {
        return [this](const drogon::HttpRequestPtr &req,
                      drogon::AdviceCallback &&callback,
                      drogon::AdviceChainCallback &&chainCallback) {
            if (req->method() == drogon::Options) {
                const auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);

                applyCorsHeaders(req, resp);

                const auto &origin = req->getHeader("Origin");
                if (!origin.empty() && isOriginAllowed(origin)) {
                    const auto &requestMethod = req->getHeader("Access-Control-Request-Method");
                    if (!requestMethod.empty()) {
                        resp->addHeader("Access-Control-Allow-Methods", requestMethod);
                    } else {
                        resp->addHeader("Access-Control-Allow-Methods",
                                        "GET, POST, PUT, PATCH, DELETE, OPTIONS");
                    }

                    const auto &requestHeaders = req->getHeader("Access-Control-Request-Headers");
                    if (!requestHeaders.empty()) {
                        resp->addHeader("Access-Control-Allow-Headers", requestHeaders);
                    } else {
                        resp->addHeader("Access-Control-Allow-Headers",
                                        "Content-Type, Authorization, X-Requested-With");
                    }

                    resp->addHeader("Access-Control-Max-Age", "86400");
                }

                callback(resp);
                return;
            }
            chainCallback();
        };
    }

    std::function<void(const drogon::HttpRequestPtr &,
                       const drogon::HttpResponsePtr &resp)>
    Router::corsPostHandlingAdvice() {
        return [this](const drogon::HttpRequestPtr &req,
                      const drogon::HttpResponsePtr &resp) {
            applyCorsHeaders(req, resp);
        };
    }

    drogon::HttpResponsePtr Router::default404Response() {
        static auto notFoundResp = drogon::HttpResponse::newHttpResponse();
        notFoundResp->setStatusCode(drogon::k404NotFound);
        notFoundResp->setContentTypeString("application/json");
        notFoundResp->setBody(R"({"status":404,"error":"Not Found","data":{}})");
        return notFoundResp;
    }

    std::function<void(MantisRequest &, MantisResponse &)> Router::handleAuthVerify() {
        return [](MantisRequest &req, const MantisResponse &res) {
            try {
                // Require admin authentication
                const auto &verification = req.getOr<json>("verification", json::object());

                if (verification.empty()) {
                    // Send auth error
                    res.sendJSON(401, {
                                     {"data", json::object()},
                                     {"status", 401},
                                     {"error", "Missing or invalid auth token"}
                                 });
                    return;
                }

                const bool ok = verification.contains("verified") &&
                                verification["verified"].is_boolean() &&
                                verification["verified"].get<bool>();
                if (ok) {
                    // Send verify success
                    res.sendJSON(200, {
                                     {"data", {{"status", "OK"}}},
                                     {"status", 200},
                                     {"error", ""}
                                 });
                    return;
                }

                // Send auth error
                const auto err_str = verification["error"].empty() ? "Token Verification Error" : verification["error"];
                res.sendJSON(401, {
                                 {"data", json::object()},
                                 {"status", 401},
                                 {"error", err_str}
                             });
            } catch (std::exception &e) {
                req.mbApp().logger().critical("Auth",
                                              "Auth verification error",
                                              e.what());
                // Send verify error
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"status", 500},
                                 {"error", e.what()}
                             });
            }
        };
    }

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
                const auto entity = req.mbApp().entity(entity_name);

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
                                     {
                                         "error",
                                         "This account uses OAuth login. Please sign in with your linked provider."
                                     }
                                 });
                    return;
                }

                if (!verifyPassword(body["password"].get<std::string>(), user["password"].get<std::string>())) {
                    res.sendJSON(404, {
                                     {"status", 404},
                                     {"data", json::object()},
                                     {
                                         "error",
                                         "No user found matching given `identity`, `password` and `entity` combination."
                                     }
                                 });
                    auto _body = body;
                    _body.erase("password");
                    req.mbApp().logger().warn("Auth", "User Not Found",
                                              fmt::format("No user found matching given data: \n\t- {}", _body.dump()));
                    return;
                }

                auto token = req.mbApp().auth().createToken({{"id", user["id"]}, {"entity", entity.name()}});

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

                const auto entity = req.mbApp().entity("mb_admins");

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
                                     {
                                         "error",
                                         "No user found matching given `identity`, `password` and `entity` combination."
                                     }
                                 });
                    auto _body = body;
                    _body.erase("password");
                    req.mbApp().logger().warn("Auth", "Admin User Not Found",
                                              fmt::format("No user found matching given data: \n\t- {}", _body.dump()));
                    return;
                }

                auto token = req.mbApp().auth().createToken({{"id", user["id"]}, {"entity", entity.name()}});

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

    std::function<void(MantisRequest &, MantisResponse &)> Router::handleAuthRefresh() const {
        return [](MantisRequest &req, const MantisResponse &res) {
            try {
                auto auth = req.getOr<json>("auth", json::object());
                auto verification = req.getOr<json>("verification", json::object());

                if (!verification.contains("verified") || !verification["verified"].get<bool>()) {
                    res.sendJSON(401, {
                                     {"status", 401},
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

                auto result = req.mbApp().auth().refreshSession(session_id, entity_name, user_id);

                // Get user record
                const auto entity = req.mbApp().entity(entity_name);
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
                    res.sendJSON(401, {
                                     {"status", 401},
                                     {"data", json::object()},
                                     {"error", "Valid token required to logout"}
                                 });
                    return;
                }

                auto claims = verification["claims"];
                auto session_id = claims.value("session_id", "");

                if (session_id.empty()) {
                    res.sendJSON(500, {
                                     {"status", 500},
                                     {"data", nullptr},
                                     {"error", "Missing session id"}
                                 });
                    return;
                }
                if (req.mbApp().auth().deleteSession(session_id))
                    res.sendJSON(200, {
                                     {"status", 200},
                                     {"data", {{"logged_out", true}}},
                                     {"error", ""}
                                 });
                else
                    res.sendJSON(500, {
                                     {"status", 500},
                                     {"data", nullptr},
                                     {"error", "Failed to delete session"}
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
            try {
                auto auth = req.getOr("auth", json::object());
                req.mbApp().logger().trace("Auth", "Auth Data", fmt::format("Auth Data: {}", auth.dump()));

                auto verification = req.getOr<json>("verification", json::object());
                if (verification.empty()) {
                    res.sendJSON(401, {
                                     {"data", json::object()},
                                     {"status", 401},
                                     {"error", "Auth required to access this resource!"}
                                 });
                    return;
                }

                const bool verified = verification.contains("verified") &&
                                      verification["verified"].is_boolean() &&
                                      verification["verified"].get<bool>();

                if (!verified) {
                    res.sendJSON(401, {
                                     {"data", json::object()},
                                     {"status", 401},
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

                const auto entity = req.mbApp().entity("mb_service_acc");
                const auto admin_entity = req.mbApp().entity("mb_admins");

                const auto &[body, err] = req.getBodyAsJson();

                if (!err.empty()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"status", 400},
                                     {"error", err}
                                 });
                    return;
                }

                if (const auto v_err = Validators::validateRequestBody(admin_entity, body);
                    v_err.has_value()) {
                    req.mbApp().logger().critical("Request Validation Error",
                                                  fmt::format("Error validating request body\n\t- {}", v_err.value()));
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
