#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/router.h"
#include "../../../include/mantisbase/scripting/scripting_engine.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRouterBindings(duk_context *ctx) {
        dukglue_register_method_varargs(ctx, &Router::bindRoute, "addRoute");
    }
}

#endif // MB_SCRIPTING_ENABLED
