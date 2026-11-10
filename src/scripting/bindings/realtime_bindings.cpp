#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/realtime.h"
#include "../../../include/mantisbase/core/router.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRealtimeBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbRealtimeDB::notifyChange, "notifyChange");
        dukglue_register_method(ctx, &MbRouter::broadcastChangeJson, "broadcastChange");
    }
}

#endif // MB_SCRIPTING_ENABLED
