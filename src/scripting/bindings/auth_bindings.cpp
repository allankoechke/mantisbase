#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/auth.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerAuthBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbAuth::createTokenJson, "createToken");
        dukglue_register_method(ctx, &MbAuth::verifyTokenJson, "verifyToken");
        dukglue_register_method(ctx, &MbAuth::deleteSession, "deleteSession");
        dukglue_register_method(ctx, &MbAuth::refreshSessionJson, "refreshSession");
        dukglue_register_method(ctx, &MbAuth::sessionTimeoutSeconds, "sessionTimeoutSeconds");
    }
}

#endif // MB_SCRIPTING_ENABLED
