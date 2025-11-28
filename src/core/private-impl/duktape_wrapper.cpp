#include "../../include/mantis/core/private-impl/http.h"
#include <iostream>

namespace mantis
{
    duk_ret_t DuktapeImpl::nativeConsoleInfo(duk_context* ctx)
    {
        duk_idx_t n = duk_get_top(ctx);
        std::cout << "[INFO] ";
        for (duk_idx_t i = 0; i < n; i++) {
            if (i > 0) std::cout << " ";
            std::cout << duk_safe_to_string(ctx, i);
        }
        std::cout << std::endl;
        return 0;
    }

    duk_ret_t DuktapeImpl::nativeConsoleTrace(duk_context* ctx)
    {
        duk_idx_t n = duk_get_top(ctx);
        std::cout << "[TRACE] ";
        for (duk_idx_t i = 0; i < n; i++) {
            if (i > 0) std::cout << " ";
            std::cout << duk_safe_to_string(ctx, i);
        }
        std::cout << std::endl;
        return 0;
    }

    duk_ret_t DuktapeImpl::nativeConsoleTable(duk_context* ctx)
    {
        duk_idx_t n = duk_get_top(ctx);
        std::cout << "[TABLE] ";
        for (duk_idx_t i = 0; i < n; i++) {
            if (i > 0) std::cout << " ";
            std::cout << duk_safe_to_string(ctx, i);
        }
        std::cout << std::endl;
        return 0;
    }
} // mantis