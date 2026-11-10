#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include <iostream>

namespace mb {
    duk_ret_t MbDuktapeImpl::nativeConsoleInfo(duk_context *ctx) {
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

    duk_ret_t MbDuktapeImpl::nativeConsoleTrace(duk_context *ctx) {
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

    duk_ret_t MbDuktapeImpl::nativeConsoleTable(duk_context *ctx) {
        return nativeConsoleInfo(ctx);
    }

    void MbScriptingHooks::fireOnServerStart(duk_context *ctx) {
        duk_get_global_string(ctx, "onServerStart");
        if (duk_is_function(ctx, -1)) {
            if (duk_pcall(ctx, 0) != 0) {
                std::cerr << "[SCRIPT] onServerStart error: "
                          << duk_safe_to_string(ctx, -1) << std::endl;
            }
        }
        duk_pop(ctx);
    }

    void MbScriptingHooks::fireOnServerShutdown(duk_context *ctx) {
        duk_get_global_string(ctx, "onServerShutdown");
        if (duk_is_function(ctx, -1)) {
            if (duk_pcall(ctx, 0) != 0) {
                std::cerr << "[SCRIPT] onServerShutdown error: "
                          << duk_safe_to_string(ctx, -1) << std::endl;
            }
        }
        duk_pop(ctx);
    }

    void MbScriptingHooks::fireOnRecordCreated(duk_context *ctx,
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

    void MbScriptingHooks::fireOnRecordUpdated(duk_context *ctx,
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

        duk_push_c_function(ctx, &MbDuktapeImpl::nativeConsoleInfo, DUK_VARARGS);
        duk_put_prop_string(ctx, -2, "info");

        duk_push_c_function(ctx, &MbDuktapeImpl::nativeConsoleTrace, DUK_VARARGS);
        duk_put_prop_string(ctx, -2, "trace");

        duk_push_c_function(ctx, &MbDuktapeImpl::nativeConsoleInfo, DUK_VARARGS);
        duk_put_prop_string(ctx, -2, "log");

        duk_put_global_string(ctx, "console");
    }
}

#endif // MB_SCRIPTING_ENABLED
