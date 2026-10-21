#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/logger/logger.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerLogsBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbLogger::jsInfo, "info");
        dukglue_register_method(ctx, &MbLogger::jsWarn, "warn");
        dukglue_register_method(ctx, &MbLogger::jsError, "error");
        dukglue_register_method(ctx, &MbLogger::jsDebug, "debug");
        dukglue_register_method(ctx, &MbLogger::jsTrace, "trace");
    }
}

#endif // MB_SCRIPTING_ENABLED
