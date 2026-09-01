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
                                MantisResponse &res) const {
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

    duk_ret_t Router::bindRoute(duk_context *ctx) {
        auto method = trim(duk_require_string(ctx, 0));
        std::ranges::transform(method, method.begin(), ::toupper);
        if (method.empty()
            || !(method == "GET" || method == "POST" || method == "PATCH" || method == "DELETE")) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR,
                      "addRoute expects request method of type `GET`, `POST`, `PATCH` or `DELETE` only!");
            return DUK_RET_TYPE_ERROR;
        }

        const auto path = trim(duk_require_string(ctx, 1));
        if (path.empty() || path[0] != '/') {
            duk_error(ctx, DUK_ERR_TYPE_ERROR,
                      "addRoute expects route paths to be valid and start with `/`!");
            return DUK_RET_TYPE_ERROR;
        }

        const duk_idx_t n = duk_get_top(ctx);
        if (n < 3) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "addRoute requires at least a handler function");
            return DUK_RET_TYPE_ERROR;
        }

        if (!duk_is_callable(ctx, 2)) {
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument 2 must be a callable handler function");
            return DUK_RET_TYPE_ERROR;
        }

        duk_dup(ctx, 2);
        DukValue handler = DukValue::take_from_stack(ctx);

        std::vector<DukValue> middlewares;
        for (duk_idx_t i = 3; i < n; i++) {
            if (!duk_is_callable(ctx, i)) {
                duk_error(ctx, DUK_ERR_TYPE_ERROR,
                          "All arguments after handler must be callable functions");
                return DUK_RET_TYPE_ERROR;
            }
            duk_dup(ctx, i);
            middlewares.push_back(DukValue::take_from_stack(ctx));
        }

        if (method == "GET") {
            Get(path, [this, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                executeJsRoute(handler, middlewares, req, res);
            });
        } else if (method == "POST") {
            Post(path, [this, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                executeJsRoute(handler, middlewares, req, res);
            });
        } else if (method == "PATCH") {
            Patch(path, [this, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                executeJsRoute(handler, middlewares, req, res);
            });
        } else if (method == "DELETE") {
            Delete(path, [this, handler, middlewares](MantisRequest &req, MantisResponse &res) {
                executeJsRoute(handler, middlewares, req, res);
            });
        } else {
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported HTTP method: %s", method.c_str());
            return DUK_RET_TYPE_ERROR;
        }

        mbApp().logger().debug("Scripting", fmt::format("Registered JS route {} {}", method, path));
        return 0;
    }
}

#endif // MB_SCRIPTING_ENABLED
