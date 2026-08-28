#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include <iostream>

namespace mb {
    duk_ret_t DuktapeImpl::nativeConsoleInfo(duk_context *ctx) {
        const duk_idx_t n = duk_get_top(ctx);
        std::cout << "[INFO] ";
        for (duk_idx_t i = 0; i < n; i++) {
            if (i > 0) {
                std::cout << " ";
            }
            std::cout << duk_safe_to_string(ctx, i);
        }
        std::cout << std::endl;
        return 0;
    }

    duk_ret_t DuktapeImpl::nativeConsoleTrace(duk_context *ctx) {
        const duk_idx_t n = duk_get_top(ctx);
        std::cout << "[TRACE] ";
        for (duk_idx_t i = 0; i < n; i++) {
            if (i > 0) {
                std::cout << " ";
            }
            std::cout << duk_safe_to_string(ctx, i);
        }
        std::cout << std::endl;
        return 0;
    }

    duk_ret_t DuktapeImpl::nativeConsoleTable(duk_context *ctx) {
        return nativeConsoleInfo(ctx);
    }

    void ScriptingHooks::fireOnServerStart(duk_context *ctx) {
        duk_get_global_string(ctx, "onServerStart");
        if (duk_is_function(ctx, -1)) {
            if (duk_pcall(ctx, 0) != 0) {
                std::cerr << "[SCRIPT] onServerStart error: "
                          << duk_safe_to_string(ctx, -1) << std::endl;
            }
        }
        duk_pop(ctx);
    }

    void ScriptingHooks::fireOnRecordCreated(duk_context *ctx,
                                             const std::string &entity,
                                             const std::string &recordId) {
        duk_get_global_string(ctx, "onRecordCreated");
        if (duk_is_function(ctx, -1)) {
            duk_push_string(ctx, entity.c_str());
            duk_push_string(ctx, recordId.c_str());
            if (duk_pcall(ctx, 2) != 0) {
                std::cerr << "[SCRIPT] onRecordCreated error: "
                          << duk_safe_to_string(ctx, -1) << std::endl;
            }
        }
        duk_pop(ctx);
    }

    void ScriptingHooks::fireOnRecordUpdated(duk_context *ctx,
                                             const std::string &entity,
                                             const std::string &recordId) {
        duk_get_global_string(ctx, "onRecordUpdated");
        if (duk_is_function(ctx, -1)) {
            duk_push_string(ctx, entity.c_str());
            duk_push_string(ctx, recordId.c_str());
            if (duk_pcall(ctx, 2) != 0) {
                std::cerr << "[SCRIPT] onRecordUpdated error: "
                          << duk_safe_to_string(ctx, -1) << std::endl;
            }
        }
        duk_pop(ctx);
    }

    void registerConsoleBindings(duk_context *ctx) {
        duk_push_object(ctx);

        duk_push_c_function(ctx, &DuktapeImpl::nativeConsoleInfo, DUK_VARARGS);
        duk_put_prop_string(ctx, -2, "info");

        duk_push_c_function(ctx, &DuktapeImpl::nativeConsoleTrace, DUK_VARARGS);
        duk_put_prop_string(ctx, -2, "trace");

        duk_push_c_function(ctx, &DuktapeImpl::nativeConsoleInfo, DUK_VARARGS);
        duk_put_prop_string(ctx, -2, "log");

        duk_put_global_string(ctx, "console");
    }
}

#endif // MB_SCRIPTING_ENABLED
