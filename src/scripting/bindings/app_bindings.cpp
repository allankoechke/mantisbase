#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/mantisbase.h"
#include "../../../include/mantisbase/scripting/scripting_engine.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerAppBindings(duk_context *ctx, MantisBase *app) {
        dukglue_register_global(ctx, app, "app");

        dukglue_register_property(ctx, &MantisBase::host, &MantisBase::setHost, "host");
        dukglue_register_property(ctx, &MantisBase::port, &MantisBase::setPort, "port");
        dukglue_register_property(ctx, &MantisBase::poolSize, &MantisBase::setPoolSize, "poolSize");
        dukglue_register_property(ctx, &MantisBase::publicDir, &MantisBase::setPublicDir, "publicDir");
        dukglue_register_property(ctx, &MantisBase::dataDir, &MantisBase::setDataDir, "dataDir");
        dukglue_register_property(ctx, &MantisBase::isDevMode, nullptr, "devMode");
        dukglue_register_property(ctx, &MantisBase::dbType, nullptr, "dbType");
        dukglue_register_property(ctx, &MantisBase::jwtSecretKey_JSWrapper, nullptr, "secretKey");
        dukglue_register_property(ctx, &MantisBase::version_JSWrapper, nullptr, "version");

        dukglue_register_method(ctx, &MantisBase::close, "close");
        dukglue_register_method(ctx, &MantisBase::quit_JSWrapper, "quit");
        dukglue_register_method(ctx, &MantisBase::duk_db, "db");
        dukglue_register_method(ctx, &MantisBase::duk_router, "router");
        dukglue_register_method(ctx, &MantisBase::duk_settings, "settings");
        dukglue_register_method(ctx, &MantisBase::duk_auth, "auth");
        dukglue_register_method(ctx, &MantisBase::duk_files, "files");
        dukglue_register_method(ctx, &MantisBase::duk_logs, "logs");
        dukglue_register_method(ctx, &MantisBase::duk_rt, "rt");
        dukglue_register_method(ctx, &MantisBase::loadScriptJs, "loadScript");
    }
}

#endif // MB_SCRIPTING_ENABLED
