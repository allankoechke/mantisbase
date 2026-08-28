#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/http.h"
#include "../../../include/mantisbase/core/middlewares.h"
#include "../../../include/mantisbase/scripting/scripting_engine.h"

#include <dukglue/dukglue.h>
#include <memory>

namespace mb {
    namespace {
        struct MiddlewareHolder {
            MiddlewareFn fn;
        };

        duk_ret_t run_middleware(duk_context *ctx) {
            duk_push_current_function(ctx);
            duk_get_prop_string(ctx, -1, "\xFF" "mw");
            auto *holder = static_cast<MiddlewareHolder *>(duk_get_pointer(ctx, -1));
            duk_pop_n(ctx, 2);

            if (!holder) {
                duk_push_boolean(ctx, false);
                return 1;
            }

            MantisRequest *req = nullptr;
            MantisResponse *res = nullptr;
            dukglue_read(ctx, 0, &req);
            dukglue_read(ctx, 1, &res);

            const bool ok = holder->fn(*req, *res) == HandlerResponse::Unhandled;
            duk_push_boolean(ctx, ok);
            return 1;
        }

        duk_ret_t push_middleware(duk_context *ctx, MiddlewareFn fn) {
            auto *holder = new MiddlewareHolder{std::move(fn)};
            duk_push_c_function(ctx, run_middleware, 2);
            duk_push_pointer(ctx, holder);
            duk_put_prop_string(ctx, -2, "\xFF" "mw");
            return 1;
        }

        duk_ret_t mw_getAuthToken(duk_context *ctx) { return push_middleware(ctx, getAuthToken()); }
        duk_ret_t mw_hydrateContextData(duk_context *ctx) { return push_middleware(ctx, hydrateContextData()); }
        duk_ret_t mw_resolveSchema(duk_context *ctx) { return push_middleware(ctx, resolveSchema()); }
        duk_ret_t mw_resolveAuthEntity(duk_context *ctx) { return push_middleware(ctx, resolveAuthEntity()); }
        duk_ret_t mw_resolveEntity(duk_context *ctx) { return push_middleware(ctx, resolveEntity()); }
        duk_ret_t mw_rejectViewMutations(duk_context *ctx) { return push_middleware(ctx, rejectViewMutations()); }
        duk_ret_t mw_hasEntityAccess(duk_context *ctx) { return push_middleware(ctx, hasEntityAccess()); }
        duk_ret_t mw_requireGuestOnly(duk_context *ctx) { return push_middleware(ctx, requireGuestOnly()); }
        duk_ret_t mw_requireAdminAuth(duk_context *ctx) { return push_middleware(ctx, requireAdminAuth()); }

        duk_ret_t mw_hasAccess(duk_context *ctx) {
            const auto entity = std::string(duk_require_string(ctx, 0));
            return push_middleware(ctx, hasAccess(entity));
        }

        duk_ret_t mw_requireExprEval(duk_context *ctx) {
            const auto expr = std::string(duk_require_string(ctx, 0));
            return push_middleware(ctx, requireExprEval(expr));
        }

        duk_ret_t mw_settingsFeatureGate(duk_context *ctx) {
            const auto key = std::string(duk_require_string(ctx, 0));
            return push_middleware(ctx, settingsFeatureGate(key));
        }

        duk_ret_t mw_requireEntityAuth(duk_context *ctx) {
            const auto entity = std::string(duk_require_string(ctx, 0));
            return push_middleware(ctx, requireEntityAuth(entity));
        }

        duk_ret_t mw_requireAdminOrEntityAuth(duk_context *ctx) {
            const auto entity = std::string(duk_require_string(ctx, 0));
            return push_middleware(ctx, requireAdminOrEntityAuth(entity));
        }
    }

    void registerMiddlewareBindings(duk_context *ctx) {
        duk_push_object(ctx);

        duk_push_c_function(ctx, mw_getAuthToken, 0);
        duk_put_prop_string(ctx, -2, "getAuthToken");

        duk_push_c_function(ctx, mw_hydrateContextData, 0);
        duk_put_prop_string(ctx, -2, "hydrateContextData");

        duk_push_c_function(ctx, mw_resolveSchema, 0);
        duk_put_prop_string(ctx, -2, "resolveSchema");

        duk_push_c_function(ctx, mw_resolveAuthEntity, 0);
        duk_put_prop_string(ctx, -2, "resolveAuthEntity");

        duk_push_c_function(ctx, mw_resolveEntity, 0);
        duk_put_prop_string(ctx, -2, "resolveEntity");

        duk_push_c_function(ctx, mw_rejectViewMutations, 0);
        duk_put_prop_string(ctx, -2, "rejectViewMutations");

        duk_push_c_function(ctx, mw_hasEntityAccess, 0);
        duk_put_prop_string(ctx, -2, "hasEntityAccess");

        duk_push_c_function(ctx, mw_requireGuestOnly, 0);
        duk_put_prop_string(ctx, -2, "requireGuestOnly");

        duk_push_c_function(ctx, mw_requireAdminAuth, 0);
        duk_put_prop_string(ctx, -2, "requireAdminAuth");

        duk_push_c_function(ctx, mw_hasAccess, 1);
        duk_put_prop_string(ctx, -2, "hasAccess");

        duk_push_c_function(ctx, mw_requireExprEval, 1);
        duk_put_prop_string(ctx, -2, "requireExprEval");

        duk_push_c_function(ctx, mw_settingsFeatureGate, 1);
        duk_put_prop_string(ctx, -2, "settingsFeatureGate");

        duk_push_c_function(ctx, mw_requireEntityAuth, 1);
        duk_put_prop_string(ctx, -2, "requireEntityAuth");

        duk_push_c_function(ctx, mw_requireAdminOrEntityAuth, 1);
        duk_put_prop_string(ctx, -2, "requireAdminOrEntityAuth");

        duk_put_global_string(ctx, "middlewares");
    }
}

#endif // MB_SCRIPTING_ENABLED
