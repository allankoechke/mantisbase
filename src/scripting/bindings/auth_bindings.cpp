#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/auth.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerAuthBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &Auth::createTokenJson, "createToken");
        dukglue_register_method(ctx, &Auth::verifyTokenJson, "verifyToken");
        dukglue_register_method(ctx, &Auth::deleteSession, "deleteSession");
        dukglue_register_method(ctx, &Auth::refreshSessionJson, "refreshSession");
        dukglue_register_method(ctx, &Auth::sessionTimeoutSeconds, "sessionTimeoutSeconds");
    }
}

#endif // MB_SCRIPTING_ENABLED
