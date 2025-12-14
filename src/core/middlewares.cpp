#include "../include/mantisbase/core/middlewares.h"
#include "../include/mantisbase/mantis.h"
#include "../include/mantisbase/core/models/entity.h"

namespace mb {
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

                // Update context data and exit from middleware if not verified
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

                logger::trace("Authenticated on entity {} as user with id {}", user_table, user_id);

                try {
                    const auto user_entity = MantisBase::instance().entity(user_table);
                    if (auto user = user_entity.read(user_id); user.has_value()) {
                        auth["user"] = user.value();
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
            try {
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

                    res.sendJSON(400, response);
                    return HandlerResponse::Handled;
                }

                // Store rule, depending on the request type
                AccessRule rule = method == "GET"
                                      ? (req.hasPathParams()
                                             ? entity.getRule()
                                             : entity.listRule())
                                      : method == "POST"
                                            ? entity.addRule()
                                            : method == "PATCH"
                                                  ? entity.updateRule()
                                                  : entity.deleteRule();

                // logger::trace("Access Rule?\n\t: {}", rule.toJSON().dump());

                if (rule.mode() == "public") {
                    logger::trace("Public access, no auth required!");
                    // Open to all
                    return HandlerResponse::Unhandled;
                }

                // For empty mode (admin only)
                if (rule.mode().empty()) {
                    logger::trace("Restricted access, admin auth required!");

                    // Require at least one valid auth on any table
                    auto verification = req.getOr<json>("verification", json::object());
                    if (verification.empty()) {
                        // Send auth error
                        res.sendJSON(403, {
                                         {"data", json::object()},
                                         {"status", 403},
                                         {"error", "Admin auth required to access this resource!"}
                                     });
                        return HandlerResponse::Handled;
                    }

                    if (verification.contains("verified") &&
                        verification["verified"].is_boolean() &&
                        verification["verified"].get<bool>()) {

                        // Check if verified user object is valid, if not throw auth error
                        if (auth["user"].is_null() || !auth["user"].is_object()) {
                            res.sendJSON(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", "Auth user not found!"}
                                 });

                            return HandlerResponse::Handled;
                        }

                        return HandlerResponse::Unhandled;
                    }

                    // Send auth error
                    res.sendJSON(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", verification["error"]}
                                 });
                    return HandlerResponse::Handled;
                }

                if (rule.mode() == "auth" || (auth["entity"].is_string() && auth["entity"].get<std::string>() == "mb_admins")) {
                    logger::trace("Restricted access, admin/user auth required!");

                    // Require at least one valid auth on any table
                    auto verification = req.getOr<json>("verification", json::object());
                    if (verification.empty()) {
                        // Send auth error
                        res.sendJSON(403, {
                                         {"data", json::object()},
                                         {"status", 403},
                                         {"error", "Auth required to access this resource!"}
                                     });
                        return HandlerResponse::Handled;
                    }

                    if (verification.contains("verified") &&
                        verification["verified"].is_boolean() &&
                        verification["verified"].get<bool>()) {

                        // Check if verified user object is valid, if not throw auth error
                        if (auth["user"].is_null() || !auth["user"].is_object()) {
                            res.sendJSON(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", "Auth user not found!"}
                                 });
                            return HandlerResponse::Handled;
                        }

                        return HandlerResponse::Unhandled;
                    }

                    // Send auth error
                    res.sendJSON(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", verification["error"]}
                                 });
                    return HandlerResponse::Handled;
                }

                if (rule.mode() == "custom") {
                    logger::trace("Restricted access, custom expression `{}` to be evaluated", rule.expr());

                    // Evaluate expression
                    const std::string expr = rule.expr();

                    // Token map variables for evaluation
                    TokenMap vars;

                    // Add `auth` data to the TokenMap
                    vars["auth"] = auth;

                    // Request Token Map
                    json req_obj;
                    req_obj["remoteAddr"] = req.getRemoteAddr();
                    req_obj["remotePort"] = req.getRemotePort();
                    req_obj["localAddr"] = req.getLocalAddr();
                    req_obj["localPort"] = req.getLocalPort();
                    req_obj["body"] = json::object();

                    try {
                        // TODO handle form data
                        if (req.getMethod() == "POST" && !req.getBody().empty()) {
                            // Parse request body and add it to the request TokenMap
                            req_obj["body"] = req.getBodyAsJson();
                        }
                    } catch (...) {
                    }

                    // Add the request map to the vars
                    vars["req"] = req_obj;

                    // If expression evaluation returns true, lets return allowing execution
                    // continuation. Else, we'll craft an error response.
                    if (Expr::eval(expr, vars))
                        return HandlerResponse::Unhandled; // Proceed to next middleware

                    // Evaluation yielded false, return generic access denied error
                    json response;
                    response["status"] = 403;
                    response["data"] = json::object();
                    response["error"] = "Access denied!";

                    res.sendJSON(403, response);
                    return HandlerResponse::Handled;
                }

                res.sendJSON(403, {
                                 {"status", 403},
                                 {"data", json::object()},
                                 {"error", "Access denied, entity access rule unknown."}
                             });
                return HandlerResponse::Handled;
            } catch (std::exception &e) {
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"data", json::object()},
                                 {"error", e.what()}
                             });
                return HandlerResponse::Handled;
            }
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

            res.sendJSON(403, {
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
                logger::trace("Verification: {}", verification.dump());

                if (verification.empty()) {
                    // Send auth error
                    res.sendJSON(403, {
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
                    logger::trace("Ver User Auth: {}", auth.dump());

                    // Check if verified user object is valid, if not throw auth error
                    if (auth["user"].is_null() || !auth["user"].is_object()) {
                        res.sendJSON(403, {
                                 {"data", json::object()},
                                 {"status", 403},
                                 {"error", "Auth user not found!"}
                             });
                        return HandlerResponse::Handled;
                    }

                    // logger::trace("Auth: {}", auth.dump());
                    // Ensure the auth user was for admin table
                    if (auth["entity"].get<std::string>() == "mb_admins") {
                        return HandlerResponse::Unhandled;
                    }

                    // Send auth error
                    res.sendJSON(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", "Admin auth required to access this resource."}
                                 });
                    return HandlerResponse::Handled;
                }

                // Send auth error
                const auto err_str = verification["error"].empty() ? "Token Verification Error" : verification["error"];
                res.sendJSON(403, {
                                 {"data", json::object()},
                                 {"status", 403},
                                 {"error", err_str}
                             });
                return HandlerResponse::Handled;
            } catch (std::exception &e) {
                logger::critical("Error authenticating as admin: {}", e.what());
                // Send auth error
                res.sendJSON(500, {
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
