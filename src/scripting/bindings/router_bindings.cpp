#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/router.h"
#include "../../../include/mantisbase/scripting/scripting_engine.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRouterBindings(duk_context *ctx) {
        dukglue_register_method_varargs(ctx, &Router::bindRoute, "addRoute");
        dukglue_register_method_varargs(ctx, &Router::bindGet, "get");
        dukglue_register_method_varargs(ctx, &Router::bindPost, "post");
        dukglue_register_method_varargs(ctx, &Router::bindPatch, "patch");
        dukglue_register_method_varargs(ctx, &Router::bindPut, "put");
        dukglue_register_method_varargs(ctx, &Router::bindDelete, "delete");
        dukglue_register_method_varargs(ctx, &Router::bindRedirect, "redirect");
    }
}

#endif // MB_SCRIPTING_ENABLED
