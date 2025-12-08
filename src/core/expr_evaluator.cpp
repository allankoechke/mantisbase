#include "../../include/mantisbase/core/expr_evaluator.h"
#include "../../include/mantisbase/core/logger.h"

#include <httplib.h>

namespace mantis {
    DukCtx::DukCtx() {
        // Create isolated context for this evaluation
        m_ctx = duk_create_heap_default();
    }

    DukCtx::~DukCtx() {
        // Always clean up context to ensure no state leakage
        duk_destroy_heap(m_ctx);
    }

    duk_context *DukCtx::get() const { return m_ctx; }

    bool Expr::eval(const std::string &expr, const std::unordered_map<std::string, nlohmann::json> &vars) {
        if (trim(expr).empty()) return false;

        // Create isolated context for this evaluation
        const DukCtx ctx;

        try {
            // Inject each JSON object as a global variable
            for (const auto &[key, value]: vars) {
                // Push JSON string and decode it to JavaScript object
                std::string val_str = value.dump();
                duk_push_lstring(ctx.get(), val_str.data(), val_str.length());
                duk_json_decode(ctx.get(), -1);
                duk_put_global_string(ctx.get(), key.c_str());
            }

            // Evaluate expression and return boolean result
            return dukglue_peval<bool>(ctx.get(), expr.c_str());
        } catch (const DukErrorException &e) {
            // Handle evaluation errors
            logger::critical("Error evaluating expression '{}', error: {}", expr, e.what());
            return false;
        }
    }
} // mantis
