#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/realtime.h"
#include "../../../include/mantisbase/core/router.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRealtimeBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &RealtimeDB::notifyChange, "notifyChange");
        dukglue_register_method(ctx, &Router::broadcastChangeJson, "broadcastChange");
    }
}

#endif // MB_SCRIPTING_ENABLED
