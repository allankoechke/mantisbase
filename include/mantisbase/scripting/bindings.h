/**
 * @file bindings.h
 * @brief Duktape registration entry points for MantisBase scripting.
 */

#ifndef MB_SCRIPTING_BINDINGS_H
#define MB_SCRIPTING_BINDINGS_H

#ifdef MB_SCRIPTING_ENABLED

struct duk_hthread;
typedef struct duk_hthread duk_context;

namespace mb {
    class MantisBase;

    void registerAppBindings(duk_context *ctx, MantisBase *app);
    void registerConsoleBindings(duk_context *ctx);
    void registerUtilsBindings(duk_context *ctx);
    void registerDatabaseBindings(duk_context *ctx);
    void registerRequestBindings(duk_context *ctx);
    void registerResponseBindings(duk_context *ctx);
    void registerRouterBindings(duk_context *ctx);
    void registerSettingsBindings(duk_context *ctx);
    void registerAuthBindings(duk_context *ctx);
    void registerMiddlewareBindings(duk_context *ctx);
    void registerFilesBindings(duk_context *ctx);
    void registerLogsBindings(duk_context *ctx);
    void registerRealtimeBindings(duk_context *ctx);
}

#endif // MB_SCRIPTING_ENABLED

#endif // MB_SCRIPTING_BINDINGS_H
