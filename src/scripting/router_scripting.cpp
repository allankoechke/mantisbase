#include "../../include/mantisbase/core/router.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/scripting/scripting_engine.h"
#include "../../include/mantisbase/utils/utils.h"

#ifdef MB_SCRIPTING_ENABLED

#include <algorithm>
#include <vector>

namespace mb {
    void Router::executeJsRoute(const DukValue &handler,
                                const std::vector<DukValue> &middlewares,
                                MantisRequest &req,
                                MantisResponse &res) {
        auto *engine = ScriptingEngine::active();
        if (!engine) {
            res.sendJSON(500, json{{"error", "Scripting engine not available"}, {"status", "500"}, {"data", json::object()}});
            return;
        }

        for (const auto &middleware : middlewares) {
            const bool ok = engine->pcallBool(middleware, req, res);
            if (!ok) {
                if (res.getStatus() < 400) {
                    res.setStatus(500);
                }
                return;
            }
        }

        engine->pcallVoid(handler, req, res);
    }

    void Router::broadcastChange(const nlohmann::json &change_event) const {
        sseMgr().broadcastChange(change_event);
    }

    void Router::broadcastChangeJson(const std::string &event_json) const {
        broadcastChange(json::parse(event_json));
    }

    namespace {
        /// Shared helper behind addRoute/get/post/patch/delete JS bindings.
        /// Expects (path, handler, ...middlewares) with the HTTP method fixed by the caller.
        duk_ret_t bindJsRouteForMethod(Router *router, const std::string &method, duk_context *ctx) {
            const auto path = trim(duk_require_string(ctx, 0));
            if (path.empty() || path[0] != '/') {
                duk_error(ctx, DUK_ERR_TYPE_ERROR,
                          "Route path must be valid and start with `/`!");
                return DUK_RET_TYPE_ERROR;
            }

            const duk_idx_t n = duk_get_top(ctx);
            if (n < 2) {
                duk_error(ctx, DUK_ERR_TYPE_ERROR, "Route registration requires at least a handler function");
                return DUK_RET_TYPE_ERROR;
            }

            if (!duk_is_callable(ctx, 1)) {
                duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument 1 must be a callable handler function");
                return DUK_RET_TYPE_ERROR;
            }

            duk_dup(ctx, 1);
            DukValue handler = DukValue::take_from_stack(ctx);

            std::vector<DukValue> middlewares;
            for (duk_idx_t i = 2; i < n; i++) {
                if (!duk_is_callable(ctx, i)) {
                    duk_error(ctx, DUK_ERR_TYPE_ERROR,
                              "All arguments after handler must be callable functions");
                    return DUK_RET_TYPE_ERROR;
                }
                duk_dup(ctx, i);
                middlewares.push_back(DukValue::take_from_stack(ctx));
            }

            if (method == "GET") {
                router->Get(path, [router, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                    mb::Router::executeJsRoute(handler, middlewares, req, res);
                });
            } else if (method == "POST") {
                router->Post(path, [router, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                    mb::Router::executeJsRoute(handler, middlewares, req, res);
                });
            } else if (method == "PATCH") {
                router->Patch(path, [router, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                    mb::Router::executeJsRoute(handler, middlewares, req, res);
                });
            } else if (method == "PUT") {
                router->Put(path, [router, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                    mb::Router::executeJsRoute(handler, middlewares, req, res);
                });
            } else if (method == "DELETE") {
                router->Delete(path, [router, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                    mb::Router::executeJsRoute(handler, middlewares, req, res);
                });
            } else {
                duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported HTTP method: %s", method.c_str());
                return DUK_RET_TYPE_ERROR;
            }

            router->mbApp().logger().debug("Scripting", fmt::format("Registered JS route {} {}", method, path));
            return 0;
        }
    } // namespace

    duk_ret_t Router::bindRoute(duk_context *ctx) {
        auto method = trim(duk_require_string(ctx, 0));
        std::ranges::transform(method, method.begin(), ::toupper);
        if (method.empty()
            || !(method == "GET" || method == "POST" || method == "PATCH" || method == "PUT" || method == "DELETE")) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR,
                      "addRoute expects request method of type `GET`, `POST`, `PATCH`, `PUT`, or `DELETE` only!");
            return DUK_RET_TYPE_ERROR;
        }

        const duk_idx_t n = duk_get_top(ctx);
        if (n < 3) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "addRoute requires at least a handler function");
            return DUK_RET_TYPE_ERROR;
        }

        // Shift the stack so the helper sees (path, handler, ...middlewares).
        duk_remove(ctx, 0);
        return bindJsRouteForMethod(this, method, ctx);
    }

    duk_ret_t Router::bindGet(duk_context *ctx) {
        return bindJsRouteForMethod(this, "GET", ctx);
    }

    duk_ret_t Router::bindPost(duk_context *ctx) {
        return bindJsRouteForMethod(this, "POST", ctx);
    }

    duk_ret_t Router::bindPatch(duk_context *ctx) {
        return bindJsRouteForMethod(this, "PATCH", ctx);
    }

    duk_ret_t Router::bindPut(duk_context *ctx) {
        return bindJsRouteForMethod(this, "PUT", ctx);
    }

    duk_ret_t Router::bindDelete(duk_context *ctx) {
        return bindJsRouteForMethod(this, "DELETE", ctx);
    }

    duk_ret_t Router::bindRedirect(duk_context *ctx) const {
        const duk_idx_t n = duk_get_top(ctx);
        if (n < 2) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR,
                      "redirect requires at least a source path and a destination (redirect(from, to[, status[, methods]]))");
            return DUK_RET_TYPE_ERROR;
        }

        const auto from = trim(duk_require_string(ctx, 0));
        if (from.empty() || from[0] != '/') {
            duk_error(ctx, DUK_ERR_TYPE_ERROR,
                      "redirect expects the source path to be valid and start with `/`!");
            return DUK_RET_TYPE_ERROR;
        }

        const auto to = trim(duk_require_string(ctx, 1));
        if (to.empty()) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "redirect expects a non-empty destination path or URL!");
            return DUK_RET_TYPE_ERROR;
        }

        int status = 302;
        if (n >= 3 && !duk_is_undefined(ctx, 2) && !duk_is_null(ctx, 2)) {
            if (!duk_is_number(ctx, 2)) {
                duk_error(ctx, DUK_ERR_TYPE_ERROR, "redirect status must be a 3xx number (e.g. 301, 302)!");
                return DUK_RET_TYPE_ERROR;
            }
            status = static_cast<int>(duk_require_number(ctx, 2));
            if (status < 300 || status > 399) {
                duk_error(ctx, DUK_ERR_TYPE_ERROR,
                          "redirect status must be a 3xx number (e.g. 301, 302, 307, 308)!");
                return DUK_RET_TYPE_ERROR;
            }
        }

        // Default mirrors Router::redirect: GET-only. An empty array allows all methods.
        std::vector<std::string> constraints{"GET"};
        if (n >= 4 && !duk_is_undefined(ctx, 3) && !duk_is_null(ctx, 3)) {
            constraints.clear();
            if (duk_is_string(ctx, 3)) {
                auto single = trim(duk_require_string(ctx, 3));
                if (!single.empty()) constraints.push_back(single);
            } else if (duk_is_array(ctx, 3)) {
                const auto len = duk_get_length(ctx, 3);
                for (duk_uarridx_t i = 0; i < len; ++i) {
                    duk_get_prop_index(ctx, 3, i);
                    if (!duk_is_string(ctx, -1)) {
                        duk_error(ctx, DUK_ERR_TYPE_ERROR,
                                  "redirect methods must be strings (e.g. [\"GET\", \"POST\"])!");
                        return DUK_RET_TYPE_ERROR;
                    }
                    auto entry = trim(duk_require_string(ctx, -1));
                    duk_pop(ctx);
                    if (!entry.empty()) constraints.push_back(entry);
                }
            } else {
                duk_error(ctx, DUK_ERR_TYPE_ERROR,
                          "redirect methods must be a string or an array of strings!");
                return DUK_RET_TYPE_ERROR;
            }
        }

        try {
            redirect(from, to, status, constraints);
        } catch (const std::exception &e) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "redirect failed: %s", e.what());
            return DUK_RET_TYPE_ERROR;
        }

        return 0;
    }
}

#endif // MB_SCRIPTING_ENABLED
