#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/database.h"
#include "../../../include/mantisbase/mantisbase.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerDatabaseBindings(duk_context *ctx) {
        dukglue_register_property(ctx, &Database::isConnected, nullptr, "connected");
        dukglue_register_method(ctx, &Database::session, "session");
        dukglue_register_method_varargs(ctx, &Database::query, "query");

        dukglue_register_method(ctx, &soci::session::close, "close");
        dukglue_register_method(ctx, &soci::session::reconnect, "reconnect");
        dukglue_register_property(ctx, &soci::session::is_connected, nullptr, "connected");
        dukglue_register_method(ctx, &soci::session::begin, "begin");
        dukglue_register_method(ctx, &soci::session::commit, "commit");
        dukglue_register_method(ctx, &soci::session::rollback, "rollback");
        dukglue_register_method(ctx, &soci::session::get_query, "getQuery");
        dukglue_register_method(ctx, &soci::session::get_last_query, "getLastQuery");
        dukglue_register_method(ctx, &soci::session::get_last_query_context, "getLastQueryContext");
        dukglue_register_method(ctx, &soci::session::got_data, "gotData");
        dukglue_register_method(ctx, &soci::session::get_backend_name, "getBackendName");
        dukglue_register_method(ctx, &soci::session::empty_blob, "emptyBlob");
    }
}

#endif // MB_SCRIPTING_ENABLED
