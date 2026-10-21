#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/kv_store.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerSettingsBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbKVStore::getScriptingJson, "get");
        dukglue_register_method(ctx, &MbKVStore::setScriptingJson, "set");
        dukglue_register_method(ctx, &MbKVStore::configsScriptingJson, "configs");
        dukglue_register_method(ctx, &MbKVStore::reloadScriptingConfigs, "reload");
    }
}

#endif // MB_SCRIPTING_ENABLED
