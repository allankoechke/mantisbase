#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/kv_store.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerSettingsBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &KeyValStore::getScriptingJson, "get");
        dukglue_register_method(ctx, &KeyValStore::setScriptingJson, "set");
        dukglue_register_method(ctx, &KeyValStore::configsScriptingJson, "configs");
        dukglue_register_method(ctx, &KeyValStore::reloadScriptingConfigs, "reload");
    }
}

#endif // MB_SCRIPTING_ENABLED
