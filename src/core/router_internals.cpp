#include "../../include/mantisbase/core/router.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/models/validators.h"
#include "../../include/mantisbase/utils/utils.h"
#include "drogon/drogon_callbacks.h"

#include <sstream>
#include <string_view>

namespace mb {
    namespace {
        std::vector<std::string> parseAllowedOrigins() {
            std::vector<std::string> origins;
            auto env_origins = getEnvOrDefault("MB_ALLOWED_ORIGINS", "");
            if (env_origins.empty()) return origins;
            std::istringstream stream(env_origins);
            std::string origin;
            while (std::getline(stream, origin, ',')) {
                auto trimmed = trim(origin);
                if (!trimmed.empty()) origins.push_back(trimmed);
            }
            return origins;
        }

        bool rejectDisabledAuthMethod(const MbRequest &req, const MbResponse &res, const std::string_view method) {
            const auto entity_name = trim(req.pathParamValue("entity_name"));
            const auto reason = req.mbApp().entity(entity_name).authMethodDisabledReason(method);
            if (reason.empty())
                return false;
            res.sendJSON(403, {
                             {"status", 403},
                             {"data", json::object()},
                             {"error", reason}
                         });
            return true;
        }

        bool isOriginAllowed(const std::string &origin, const std::vector<std::string> &allowed) {
            if (allowed.empty()) return false;
            for (const auto &a: allowed) {
                if (a == "*" || a == origin) return true;
            }
            return false;
        }
    }

    const std::function<drogon::HttpResponsePtr(const drogon::HttpRequestPtr &)> MbRouter::reqIdSyncAdvice() {
        return [this](const drogon::HttpRequestPtr &req) {
            // Generate and store request ID in attributes
            std::string requestId = fmt::format("req_{}", m_sfId.nextID());
            req->attributes()->insert("request_id", requestId);

            // Return nullptr to continue normal processing
            return nullptr;
        };
    }

    std::function<void(const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp)>
    MbRouter::loggerPreSendingAdvice() const {
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

    void MbRouter::reloadCorsOrigins() const {
        const auto origins = std::make_shared<std::set<std::string> >();

        const auto &cfg = mbApp().settings().configs();
        if (cfg.contains("corsAllowedOrigins") && cfg["corsAllowedOrigins"].is_array()) {
            for (const auto &item: cfg["corsAllowedOrigins"]) {
                if (item.is_string()) {
                    const auto value = trim(item.get<std::string>());
                    if (!value.empty()) {
                        origins->insert(value);
                    }
                }
            }
        }

        if (const auto raw = getEnvOrDefault("MB_CORS_ORIGINS", ""); !raw.empty()) {
            for (const auto &part: splitString(raw, ",")) {
                const auto value = trim(part);
                if (!value.empty()) {
                    origins->insert(value);
                }
            }
        }

        // Writing m_corsAllowedOrigins
        {
            std::unique_lock<std::shared_mutex> lock(m_corsOriginsMutex);
            m_corsAllowedOrigins = origins;
        }

        logger().info("CORS", fmt::format("Loaded {} allowed origin(s)", origins->size()));
    }

    bool MbRouter::isOriginAllowed(const std::string &origin) const {
        std::shared_lock<std::shared_mutex> lock(m_corsOriginsMutex);
        const auto allowed = m_corsAllowedOrigins;
        return allowed && allowed->contains(origin);
    }

    void MbRouter::applyCorsHeaders(const drogon::HttpRequestPtr &req,
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
    MbRouter::corsPreRoutingAdvice() {
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
    MbRouter::corsPreSendingAdvice() const {
        return [this](const drogon::HttpRequestPtr &req,
                      const drogon::HttpResponsePtr &resp) {
            applyCorsHeaders(req, resp);
            resp->addHeader("X-Content-Type-Options", "nosniff");
            resp->addHeader("X-Frame-Options", "SAMEORIGIN");
            resp->addHeader("Referrer-Policy", "strict-origin-when-cross-origin");
        };
    }

    drogon::HttpResponsePtr MbRouter::default404Response() {
        static auto notFoundResp = drogon::HttpResponse::newHttpResponse();
        notFoundResp->setStatusCode(drogon::k404NotFound);
        notFoundResp->setContentTypeString("application/json");
        notFoundResp->setBody(R"({"status":404,"error":"Not Found","data":{}})");
        return notFoundResp;
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthVerify() {
        return [](MbRequest &req, const MbResponse &res) {
            try {
                // Require admin authentication
                const auto &verification = req.get<json>("verification", json::object());

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
                req.mbApp().logger().critical("MbAuth",
                                              "MbAuth verification error",
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

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthLogin() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                if (rejectDisabledAuthMethod(req, res, "password"))
                    return;

                const auto &[body, err] = req.bodyAsJson();
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

                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto entity = req.mbApp().entity(entity_name);

                auto opt_user = entity.queryFromCols(body["identity"].get<std::string>(), {"id", "email"});
                if (!opt_user.has_value()) {
                    res.sendJSON(404, {
                                     {"status", 404},
                                     {"data", json::object()},
                                     {"error", "Invalid identity, password or entity combination."}
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
                                         "Invalid identity, password or entity combination."
                                     }
                                 });
                    auto _body = body;
                    _body.erase("password");
                    req.mbApp().logger().warn("MbAuth", "User Not Found",
                                              fmt::format("No user found matching given data: \n\t- {}", _body.dump()));
                    return;
                }

                req.mbApp().auth().rejectUnverifiedLogin(entity.name(), user);

                auto token = req.mbApp().auth().createToken({{"id", user["id"]}, {"entity", entity.name()}});

                user.erase("password");
                res.setAuthTokenCookie(token, req.mbApp().auth().sessionTimeoutSeconds(entity.name()));
                res.sendJSON(200, {
                                 {"status", 200},
                                 {"data", {{"token", token}, {"user", user}}},
                                 {"error", ""}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Login Error", fmt::format("Login error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthMethods() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto methods = req.mbApp().entity(entity_name).authMethods().toJSON();
                res.sendJSON(200, {
                                 {"status", 200},
                                 {"data", methods},
                                 {"error", ""}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Auth Methods Error",
                                              fmt::format("Auth methods error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthOtpRequest() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                if (rejectDisabledAuthMethod(req, res, "otp"))
                    return;

                const auto &[body, err] = req.bodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(500, {
                                     {"status", 500},
                                     {"data", json::object()},
                                     {"error", err}
                                 });
                    return;
                }

                if (!body.contains("email") || !body["email"].is_string() ||
                    trim(body["email"].get<std::string>()).empty()) {
                    res.sendJSON(400, {
                                     {"status", 400},
                                     {"data", json::object()},
                                     {"error", "Expected `email` key in the request body."}
                                 });
                    return;
                }

                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto result = req.mbApp().auth().requestLoginOtp(
                    entity_name, body["email"].get<std::string>());
                res.sendJSON(200, {
                                 {"status", 200},
                                 {"data", result},
                                 {"error", ""}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "OTP Request Error",
                                              fmt::format("Sign-in code request error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthOtpLogin() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                if (rejectDisabledAuthMethod(req, res, "otp"))
                    return;

                const auto &[body, err] = req.bodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(500, {
                                     {"status", 500},
                                     {"data", json::object()},
                                     {"error", err}
                                 });
                    return;
                }

                for (const auto &key: std::vector<std::string>{"email", "otp"}) {
                    if (!body.contains(key) || !body[key].is_string() ||
                        trim(body[key].get<std::string>()).empty()) {
                        res.sendJSON(400, {
                                         {"status", 400},
                                         {"data", json::object()},
                                         {"error", "Expected `" + key + "` key in the request body."}
                                     });
                        return;
                    }
                }

                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto result = req.mbApp().auth().loginWithOtp(
                    entity_name,
                    body["email"].get<std::string>(),
                    body["otp"].get<std::string>());

                const auto token = result.at("token").get<std::string>();
                res.setAuthTokenCookie(token, req.mbApp().auth().sessionTimeoutSeconds(entity_name));
                res.sendJSON(200, {
                                 {"status", 200},
                                 {"data", result},
                                 {"error", ""}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "OTP Login Error",
                                              fmt::format("Sign-in code login error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    namespace {
        void sendVerifyEmailPage(const MbResponse &res, const int status, const std::string &heading,
                                 const std::string &message) {
            res.setStatus(status);
            res.setContent(
                "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" + heading +
                "</title></head><body style=\"font-family:-apple-system,sans-serif;max-width:560px;margin:40px auto;\">"
                "<h2>" + heading + "</h2><p>" + message + "</p></body></html>",
                "text/html; charset=utf-8");
        }
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthVerifyEmailRequest() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                const auto &[body, err] = req.bodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }
                if (!body.contains("email") || !body["email"].is_string() ||
                    trim(body["email"].get<std::string>()).empty()) {
                    res.sendJSON(400, {
                                     {"status", 400},
                                     {"data", json::object()},
                                     {"error", "Expected `email` key in the request body."}
                                 });
                    return;
                }

                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto result = req.mbApp().auth().requestEmailVerification(
                    entity_name, body["email"].get<std::string>(), req.headerValue("Host"));
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {{"status", e.code()}, {"data", json::object()}, {"error", e.what()}});
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Email Verification Request Error",
                                              fmt::format("Verification email request error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthVerifyEmail() {
        return [](const MbRequest &req, const MbResponse &res) {
            const bool as_html = req.method() == "GET";
            try {
                std::string token;
                if (as_html) {
                    token = req.queryParamValue("token");
                } else {
                    const auto &[body, err] = req.bodyAsJson();
                    if (!err.empty()) {
                        res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                        return;
                    }
                    if (!body.contains("token") || !body["token"].is_string() ||
                        trim(body["token"].get<std::string>()).empty()) {
                        res.sendJSON(400, {
                                         {"status", 400},
                                         {"data", json::object()},
                                         {"error", "Expected `token` key in the request body."}
                                     });
                        return;
                    }
                    token = body["token"].get<std::string>();
                }

                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto result = req.mbApp().auth().confirmEmailVerification(entity_name, token);
                if (as_html) {
                    sendVerifyEmailPage(res, 200, "Email verified",
                                        "Your email address is verified. You can close this page and sign in.");
                    return;
                }
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const MbException &e) {
                if (as_html) {
                    sendVerifyEmailPage(res, e.code(), "Verification failed", e.what());
                    return;
                }
                res.sendJSON(e.code(), {{"status", e.code()}, {"data", json::object()}, {"error", e.what()}});
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Email Verification Error",
                                              fmt::format("Email verification error: {}", e.what()));
                if (as_html) {
                    sendVerifyEmailPage(res, 500, "Verification failed", "An internal error occurred.");
                    return;
                }
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    namespace {
        void sendPasswordResetForm(const MbResponse &res, const std::string &token) {
            res.setStatus(200);
            res.setContent(
                "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Reset password</title>"
                "<style>body{font-family:-apple-system,sans-serif;max-width:560px;margin:40px auto;padding:0 16px}"
                "label{display:block;margin:12px 0 4px}input{width:100%;padding:10px;box-sizing:border-box}"
                "button{margin-top:16px;padding:10px 16px;background:#DC2626;color:#fff;border:0;border-radius:6px;cursor:pointer}"
                ".err{color:#DC2626;margin-top:12px}</style></head><body>"
                "<h2>Reset your password</h2>"
                "<p>Choose a new password (at least 8 characters, no spaces).</p>"
                "<form id=\"reset-form\">"
                "<label for=\"password\">New password</label>"
                "<input id=\"password\" name=\"password\" type=\"password\" autocomplete=\"new-password\" required minlength=\"8\">"
                "<label for=\"confirm\">Confirm password</label>"
                "<input id=\"confirm\" name=\"confirm\" type=\"password\" autocomplete=\"new-password\" required minlength=\"8\">"
                "<button type=\"submit\">Reset password</button>"
                "<p class=\"err\" id=\"err\" hidden></p>"
                "</form>"
                "<script>"
                "const token=" + json(token).dump() + ";"
                "const form=document.getElementById('reset-form');"
                "const err=document.getElementById('err');"
                "form.addEventListener('submit',async(e)=>{"
                "e.preventDefault();"
                "err.hidden=true;"
                "const password=document.getElementById('password').value;"
                "const confirm=document.getElementById('confirm').value;"
                "if(password!==confirm){err.textContent='Passwords do not match.';err.hidden=false;return;}"
                "try{"
                "const res=await fetch(location.pathname,{method:'POST',headers:{'Content-Type':'application/json'},"
                "body:JSON.stringify({token,password})});"
                "const body=await res.json().catch(()=>({}));"
                "if(!res.ok){err.textContent=(body&&body.error)||'Could not reset password.';err.hidden=false;return;}"
                "document.body.innerHTML='<h2>Password updated</h2><p>You can close this page and sign in with your new password.</p>';"
                "}catch(ex){err.textContent='Could not reset password.';err.hidden=false;}"
                "});"
                "</script></body></html>",
                "text/html; charset=utf-8");
        }

        void sendPasswordResetPage(const MbResponse &res, const int status, const std::string &heading,
                                   const std::string &message) {
            res.setStatus(status);
            res.setContent(
                "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" + heading +
                "</title></head><body style=\"font-family:-apple-system,sans-serif;max-width:560px;margin:40px auto;\">"
                "<h2>" + heading + "</h2><p>" + message + "</p></body></html>",
                "text/html; charset=utf-8");
        }
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthPasswordResetRequest() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                const auto &[body, err] = req.bodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }
                if (!body.contains("email") || !body["email"].is_string() ||
                    trim(body["email"].get<std::string>()).empty()) {
                    res.sendJSON(400, {
                                     {"status", 400},
                                     {"data", json::object()},
                                     {"error", "Expected `email` key in the request body."}
                                 });
                    return;
                }

                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto result = req.mbApp().auth().requestPasswordReset(
                    entity_name, body["email"].get<std::string>(), req.headerValue("Host"));
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {{"status", e.code()}, {"data", json::object()}, {"error", e.what()}});
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Password Reset Request Error",
                                              fmt::format("Password reset request error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthPasswordReset() {
        return [](const MbRequest &req, const MbResponse &res) {
            const bool as_html = req.method() == "GET";
            try {
                if (as_html) {
                    const auto token = req.queryParamValue("token");
                    if (trim(token).empty()) {
                        sendPasswordResetPage(res, 400, "Reset failed", "Missing password reset token.");
                        return;
                    }
                    sendPasswordResetForm(res, token);
                    return;
                }

                const auto &[body, err] = req.bodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {{"status", 400}, {"data", json::object()}, {"error", err}});
                    return;
                }
                if (!body.contains("token") || !body["token"].is_string() ||
                    trim(body["token"].get<std::string>()).empty()) {
                    res.sendJSON(400, {
                                     {"status", 400},
                                     {"data", json::object()},
                                     {"error", "Expected `token` key in the request body."}
                                 });
                    return;
                }
                if (!body.contains("password") || !body["password"].is_string() ||
                    body["password"].get<std::string>().empty()) {
                    res.sendJSON(400, {
                                     {"status", 400},
                                     {"data", json::object()},
                                     {"error", "Expected `password` key in the request body."}
                                 });
                    return;
                }

                const auto entity_name = trim(req.pathParamValue("entity_name"));
                const auto result = req.mbApp().auth().confirmPasswordReset(
                    entity_name,
                    body["token"].get<std::string>(),
                    body["password"].get<std::string>());
                res.sendJSON(200, {{"status", 200}, {"data", result}, {"error", ""}});
            } catch (const MbException &e) {
                if (as_html) {
                    sendPasswordResetPage(res, e.code(), "Reset failed", e.what());
                    return;
                }
                res.sendJSON(e.code(), {{"status", e.code()}, {"data", json::object()}, {"error", e.what()}});
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Password Reset Error",
                                              fmt::format("Password reset error: {}", e.what()));
                if (as_html) {
                    sendPasswordResetPage(res, 500, "Reset failed", "An internal error occurred.");
                    return;
                }
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAdminLogin() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                const auto &[body, err] = req.bodyAsJson();
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
                                     {"error", "Invalid identity, password or entity combination."}
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
                                         "Invalid identity, password or entity combination."
                                     }
                                 });
                    auto _body = body;
                    _body.erase("password");
                    req.mbApp().logger().warn("MbAuth", "Admin User Not Found",
                                              fmt::format("No user found matching given data: \n\t- {}", _body.dump()));
                    return;
                }

                auto token = req.mbApp().auth().createToken({{"id", user["id"]}, {"entity", entity.name()}});

                user.erase("password");
                res.setAuthTokenCookie(token, req.mbApp().auth().sessionTimeoutSeconds(entity.name()));
                res.sendJSON(200, {
                                 {"status", 200},
                                 {"data", {{"token", token}, {"user", user}}},
                                 {"error", ""}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Admin Login Error",
                                              fmt::format("Admin login error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthRefresh() const {
        return [](MbRequest &req, const MbResponse &res) {
            try {
                auto auth = req.get<json>("auth", json::object());
                auto verification = req.get<json>("verification", json::object());

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

                const auto new_token = result["token"].get<std::string>();
                res.setAuthTokenCookie(new_token, req.mbApp().auth().sessionTimeoutSeconds(entity_name));
                res.sendJSON(200, {
                                 {"status", 200},
                                 {"data", {{"token", new_token}, {"user", user}}},
                                 {"error", ""}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Token Refresh Error",
                                              fmt::format("Token refresh error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleAuthLogout() {
        return [](MbRequest &req, const MbResponse &res) {
            try {
                auto verification = req.get<json>("verification", json::object());

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
                if (req.mbApp().auth().deleteSession(session_id)) {
                    res.clearAuthTokenCookie();
                    res.sendJSON(200, {
                                     {"status", 200},
                                     {"data", {{"logged_out", true}}},
                                     {"error", ""}
                                 });
                } else
                    res.sendJSON(500, {
                                     {"status", 500},
                                     {"data", nullptr},
                                     {"error", "Failed to delete session"}
                                 });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Logout Error", fmt::format("Logout error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }

    std::function<void(MbRequest &, MbResponse &)> MbRouter::handleSetupAdmin() {
        return [](MbRequest &req, const MbResponse &res) {
            try {
                auto auth = req.get<json>("auth", json::object());
                auto verification = req.get<json>("verification", json::object());
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
                    res.sendJSON(401, {
                                     {"data", json::object()},
                                     {"status", 401},
                                     {"error", "Invalid service account passed in!"}
                                 });
                    return;
                }

                const auto entity = req.mbApp().entity("mb_service_acc");
                const auto admin_entity = req.mbApp().entity("mb_admins");

                const auto &[body, err] = req.bodyAsJson();

                if (!err.empty()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"status", 400},
                                     {"error", err}
                                 });
                    return;
                }

                if (const auto v_err = MbValidators::validateRequestBody(admin_entity, body);
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
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbAuth", "Admin Setup Error",
                                              fmt::format("Admin setup error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."}
                             });
            }
        };
    }
}
