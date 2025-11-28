#include "../include/mantisbase/core/middlewares.h"
#include "../include/mantisbase/mantis.h"
#include "../include/mantisbase/core/models/entity.h"

namespace mantis {
    std::function<HandlerResponse(MantisRequest &, MantisResponse &)> getAuthToken() {
        std::string msg = MANTIS_FUNC();
        return [msg](MantisRequest &req, MantisResponse &_) {
            TRACE_FUNC(msg);
            // If we have an auth header, extract it into the ctx, else
            // add a guest user type. The auth if present, should have
            // the user id, auth table, etc.
            json auth;
            auth["type"] = "guest"; // or 'user' or 'admin'
            auth["token"] = nullptr; // User token from header ...
            auth["id"] = nullptr; // Hold user `id` from auth user
            auth["entity"] = nullptr; // Hold user table if valid
            auth["user"] = nullptr; // Hold hydrated user if valid

            if (req.hasHeader("Authorization")) {
                const auto token = req.getBearerTokenAuth();
                auth["token"] = trim(token);
            }

            // Update the context
            req.set("auth", auth);
            req.set("verification", json::object());
            return HandlerResponse::Unhandled;
        };
    }

    std::function<HandlerResponse(MantisRequest &, MantisResponse &)> hydrateContextData() {
        std::string msg = MANTIS_FUNC();
        return [msg](MantisRequest &req, MantisResponse &res) {
            TRACE_FUNC(msg);
            // Get the auth var from the context, resort to empty object if it's not set.
            auto auth = req.getOr<json>("auth", json::object());

            // Expand logged user if token is present and query user information if it exists
            if (auth.contains("token") && !auth["token"].is_null() && !auth["token"].empty()) {
                const auto token = auth.at("token").get<std::string>();

                // If token validation worked, lets get data from database
                const json resp = Auth::verifyToken(token);
                req.set("verification", resp);

                // Update context data and exit if not verified
                if (!resp.at("verified").get<bool>()) {
                    req.set("auth", auth); // Update the `auth` data
                    return HandlerResponse::Unhandled;
                }

                // If token is valid, try getting user record from db and populate context record
                auto claims = resp["claims"];
                const auto user_id = claims["id"].get<std::string>();
                const auto user_table = claims["entity"].get<std::string>();

                // Update auth keys ...
                auth["id"] = user_id;
                auth["entity"] = user_table;

                // Set type to user since token is valid, but user record may be invalid
                auth["type"] = "user";

                try {
                    const auto user_entity = MantisBase::instance().entity(user_table);
                    if (auto user = user_entity.read(user_id); user.has_value()) {
                        auth["user"] = user;
                    }
                } catch (...) {
                } // Ignore errors for now
            }

            req.set("auth", auth); // Update the `auth` data
            return HandlerResponse::Unhandled;
        };
    }

    std::function<HandlerResponse(MantisRequest &, MantisResponse &)> hasAccess(const std::string &entity_name) {
        std::string msg = MANTIS_FUNC();
        return [entity_name, msg](MantisRequest &req, MantisResponse &res) {
            TRACE_FUNC(msg);
            const auto entity = MantisBase::instance().entity(entity_name);

            // Get the auth var from the context, resort to empty object if it's not set.
            auto auth = req.getOr<json>("auth", json::object());

            auto method = req.getMethod();
            if (!(method == "GET"
                  || method == "POST"
                  || method == "PATCH"
                  || method == "DELETE")) {
                json response;
                response["status"] = 400;
                response["data"] = json::object();
                response["error"] = "Unsupported method `" + method + "`";

                res.sendJson(400, response);
                return HandlerResponse::Handled;
            }

            // Store rule, depending on the request type
            AccessRule rule = method == "GET"
                                  ? (req.hasPathParams()
                                         ? entity.listRule()
                                         : entity.getRule())
                                  : method == "POST"
                                        ? entity.addRule()
                                        : method == "PATCH"
                                              ? entity.updateRule()
                                              : entity.deleteRule();

            if (rule.mode() == "public") {
                // Open to all
                return HandlerResponse::Unhandled;
            }

            // For auth/empty/admin roles, ....
            if (rule.mode() == "auth" || rule.mode().empty() || auth["entity"].get<std::string>() == "mb_admins") {
                // Require at least one valid auth on any table
                auto verification = req.getOr<json>("verification", json::object());
                if (verification.empty()) {
                    // Send auth error
                    res.sendJson(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", "Auth required to access this resource!"}
                                 });
                    return HandlerResponse::Handled;
                }

                if (verification.contains("verified") &&
                    verification["verified"].is_boolean() &&
                    verification["verified"].get<bool>()) {
                    return HandlerResponse::Unhandled;
                }

                // Send auth error
                res.sendJson(403, {
                                 {"data", json::object()},
                                 {"status", 403},
                                 {"error", verification["error"]}
                             });
                return HandlerResponse::Handled;
            }

            if (rule.mode() == "custom") {
                // Evaluate expression
                const std::string expr = rule.expr();

                // Token map variables for evaluation
                // TokenMap vars;

                // Add `auth` data to the TokenMap
                // vars["auth"] = MantisBase::instance().evaluator().jsonToTokenMap(auth);

                // Request Token Map
                json reqMap;
                reqMap["remoteAddr"] = req.getRemoteAddr();
                reqMap["remotePort"] = req.getRemotePort();
                reqMap["localAddr"] = req.getLocalAddr();
                reqMap["localPort"] = req.getLocalPort();

                try {
                    if (req.getMethod() == "POST" && !req.getBody().empty()) // TODO handle formdata
                    {
                        // Parse request body and add it to the request TokenMap
                        auto request = json::parse(req.getMethod());
                        // reqMap["body"] = MantisBase::instance().evaluator().jsonToTokenMap(request);
                    }
                } catch (...) {
                }

                // Add the request map to the vars
                // vars["req"] = reqMap;

                // If expression evaluation returns true, lets return allowing execution
                // continuation. Else, we'll craft an error response.
                // if (MantisBase::instance().evaluator().evaluate(expr, vars))
                //     return REQUEST_PENDING; // Proceed to next middleware

                // Evaluation yielded false, return generic access denied error
                json response;
                response["status"] = 403;
                response["data"] = json::object();
                response["error"] = "Access denied!";

                res.sendJson(403, response);
                return REQUEST_HANDLED;
            }

            res.sendJson(403, {
                             {"status", 403},
                             {"data", json::object()},
                             {"data", "Failed to authenticate user!"}
                         });
            return HandlerResponse::Handled;
        };
    }

    std::function<HandlerResponse(MantisRequest &, MantisResponse &)> requireExprEval(const std::string &expr) {
        std::string msg = MANTIS_FUNC();
        return [expr, msg](MantisRequest &req, MantisResponse &res) {
            TRACE_FUNC(msg);
            return REQUEST_PENDING;
        };
    }

    std::function<HandlerResponse(MantisRequest &, MantisResponse &)> requireGuestOnly() {
        std::string msg = MANTIS_FUNC();
        return [msg](MantisRequest &req, MantisResponse &res) {
            TRACE_FUNC(msg);
            const auto auth = req.getOr<json>("auth", json::object());
            if (auth["type"] == "guest")
                return HandlerResponse::Unhandled;

            res.sendJson(403, {
                             {"status", 403},
                             {"data", json::object()},
                             {"data", "Only guest users allowed to access this resource."}
                         });
            return HandlerResponse::Handled;
        };
    }

    std::function<HandlerResponse(MantisRequest &, MantisResponse &)> requireAdminAuth() {
        std::string msg = MANTIS_FUNC();
        return [msg](MantisRequest &req, const MantisResponse &res) {
            TRACE_FUNC(msg);
            try {
                // Require admin authentication
                auto verification = req.getOr<json>("verification", json::object());
                // logger::trace("Verification: {}", verification.dump());
                if (verification.empty()) {
                    // Send auth error
                    res.sendJson(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", "Auth required to access this resource!"}
                                 });
                    return HandlerResponse::Handled;
                }

                const bool ok = verification.contains("verified") &&
                                verification["verified"].is_boolean() &&
                                verification["verified"].get<bool>();
                if (ok) {
                    auto auth = req.getOr<json>("auth", json::object());
                    // logger::trace("Auth: {}", auth.dump());
                    // Ensure the auth user was for admin table
                    if (auth["entity"].get<std::string>() == "mb_admins") {
                        return HandlerResponse::Unhandled;
                    }

                    // Send auth error
                    res.sendJson(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", "Admin auth required to access this resource."}
                                 });
                    return HandlerResponse::Handled;
                }

                // Send auth error
                const auto err_str = verification["error"].empty() ? "Token Verification Error" : verification["error"];
                res.sendJson(403, {
                                 {"data", json::object()},
                                 {"status", 403},
                                 {"error",}
                             });
                return HandlerResponse::Handled;
            } catch (std::exception &e) {
                logger::critical("Error authenticating as admin: {}", e.what());
                // Send auth error
                res.sendJson(500, {
                                 {"data", json::object()},
                                 {"status", 500},
                                 {"error", e.what()}
                             });
                return HandlerResponse::Handled;
            }
        };
    }

    std::function<HandlerResponse(MantisRequest &, MantisResponse &)> requireAdminOrEntityAuth(
        const std::string &entity_name) {
        std::string msg = MANTIS_FUNC();
        return [entity_name, msg](MantisRequest &req, MantisResponse &res) {
            TRACE_FUNC(msg);
            return REQUEST_PENDING;
        };
    }

    std::function<HandlerResponse(MantisRequest &, MantisResponse &)>
    requireEntityAuth(const std::string &entity_name) {
        std::string msg = MANTIS_FUNC();
        return [entity_name, msg](MantisRequest &req, MantisResponse &res) {
            TRACE_FUNC(msg);
            return REQUEST_PENDING;
        };
    }
}
