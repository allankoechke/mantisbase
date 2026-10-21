#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/router.h"
#include "../../../include/mantisbase/scripting/scripting_engine.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRouterBindings(duk_context *ctx) {
        dukglue_register_method_varargs(ctx, &MbRouter::bindRoute, "addRoute");
        dukglue_register_method_varargs(ctx, &MbRouter::bindGet, "get");
        dukglue_register_method_varargs(ctx, &MbRouter::bindPost, "post");
        dukglue_register_method_varargs(ctx, &MbRouter::bindPatch, "patch");
        dukglue_register_method_varargs(ctx, &MbRouter::bindPut, "put");
        dukglue_register_method_varargs(ctx, &MbRouter::bindDelete, "delete");
        dukglue_register_method_varargs(ctx, &MbRouter::bindRedirect, "redirect");
    }
}

#endif // MB_SCRIPTING_ENABLED
