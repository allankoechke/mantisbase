#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/logger/logger.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerLogsBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &Logger::jsInfo, "info");
        dukglue_register_method(ctx, &Logger::jsWarn, "warn");
        dukglue_register_method(ctx, &Logger::jsError, "error");
        dukglue_register_method(ctx, &Logger::jsDebug, "debug");
        dukglue_register_method(ctx, &Logger::jsTrace, "trace");
    }
}

#endif // MB_SCRIPTING_ENABLED
