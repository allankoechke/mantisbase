/**
 * @file scripting_engine.h
 * @brief Duktape VM lifecycle, mutex-guarded execution, and deadman switch.
 */

#ifndef MB_SCRIPTING_ENGINE_H
#define MB_SCRIPTING_ENGINE_H

#ifdef MB_SCRIPTING_ENABLED

#include <dukglue/dukglue.h>
#include <mutex>
#include <string>

struct duk_hthread;
typedef struct duk_hthread duk_context;

namespace mb {
    class MantisBase;
    class MantisRequest;
    class MantisResponse;

    /**
     * @brief Owns the Duktape heap and serializes all JS execution.
     *
     * One instance per @ref MantisBase. Handlers and middleware run under
     * @ref withLock because Drogon serves requests on worker threads.
     */
    class ScriptingEngine {
    public:
        explicit ScriptingEngine(MantisBase &app);
        ~ScriptingEngine();

        ScriptingEngine(const ScriptingEngine &) = delete;
        ScriptingEngine &operator=(const ScriptingEngine &) = delete;

        /** Create heap, register bindings. No-op when disabled at runtime. */
        void init();

        /** Load scriptsDir/main.mb.js (fallback index.mantis.js). */
        void loadStartScript();

        /** Load a script relative to scriptsDir. */
        void loadScript(const std::string &relativePath);

        void fireOnServerStart();
        void fireOnRecordCreated(const std::string &entity, const std::string &recordId);
        void fireOnRecordUpdated(const std::string &entity, const std::string &recordId);

        [[nodiscard]] duk_context *ctx() const { return m_ctx; }
        [[nodiscard]] bool isActive() const { return m_ctx != nullptr; }

        /** Active engine for bindings (set for lifetime of init..destroy). */
        [[nodiscard]] static ScriptingEngine *active();

        /** Run @p fn while holding the script mutex. */
        template<typename Fn>
        auto withLock(Fn &&fn) -> decltype(fn()) {
            std::lock_guard lock(m_mutex);
            return fn();
        }

        bool pcallBool(const DukValue &fn, MantisRequest &req, MantisResponse &res);
        void pcallVoid(const DukValue &fn, MantisRequest &req, MantisResponse &res);

    private:
        void registerBindings();
        void evalFile(const std::string &filePath);
        void evalString(const std::string &content, const std::string &label);
        [[nodiscard]] bool isRuntimeDisabled() const;

        MantisBase &m_app;
        duk_context *m_ctx = nullptr;
        mutable std::mutex m_mutex;
        static ScriptingEngine *s_active;
    };
}

#endif // MB_SCRIPTING_ENABLED
#endif // MB_SCRIPTING_ENGINE_H
