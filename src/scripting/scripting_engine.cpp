#include "../../include/mantisbase/scripting/scripting_engine.h"

#ifdef MB_SCRIPTING_ENABLED

#include <duktape.h>
#include <dukglue/dukglue.h>

#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/scripting/bindings.h"
#include "../../include/mantisbase/core/http.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace mb {
    ScriptingEngine *ScriptingEngine::s_active = nullptr;

    ScriptingEngine::ScriptingEngine(MantisBase &app) : m_app(app) {}

    ScriptingEngine::~ScriptingEngine() {
        if (s_active == this) {
            s_active = nullptr;
        }
        if (m_ctx) {
            duk_destroy_heap(m_ctx);
            m_ctx = nullptr;
        }
    }

    ScriptingEngine *ScriptingEngine::active() {
        return s_active;
    }

    bool ScriptingEngine::isRuntimeDisabled() const {
        if (m_app.isScriptingDisabled()) {
            return true;
        }
        if (getEnvOrDefault("MB_SCRIPTING_DISABLED", "0") == "1") {
            return true;
        }
        return false;
    }

    void ScriptingEngine::init() {
        if (isRuntimeDisabled()) {
            m_app.logger().warn(
                "Scripting",
                "JavaScript scripting disabled (MB_SCRIPTING_DISABLED or --disable-scripting).");
            return;
        }

        m_ctx = duk_create_heap_default();
        if (!m_ctx) {
            m_app.logger().critical("Scripting", "Failed to create Duktape heap");
            return;
        }

        s_active = this;
        registerBindings();
    }

    void ScriptingEngine::registerBindings() {
        registerAppBindings(m_ctx, &m_app);
        registerRequestBindings(m_ctx);
        registerResponseBindings(m_ctx);
        registerConsoleBindings(m_ctx);
        registerUtilsBindings(m_ctx);
        registerDatabaseBindings(m_ctx);
        registerRouterBindings(m_ctx);
        registerSettingsBindings(m_ctx);
        registerAuthBindings(m_ctx);
        registerMiddlewareBindings(m_ctx);
        registerFilesBindings(m_ctx);
        registerLogsBindings(m_ctx);
        registerRealtimeBindings(m_ctx);
    }

    void ScriptingEngine::loadStartScript() {
        if (!m_ctx) {
            return;
        }

        const auto scripts_dir = fs::path(m_app.scriptsDir());
        const auto main_script = scripts_dir / "main.mb.js";
        const auto legacy_script = scripts_dir / "index.mantis.js";

        if (fs::exists(main_script)) {
            evalFile(main_script.string());
            return;
        }

        if (fs::exists(legacy_script)) {
            m_app.logger().warn(
                "Scripting",
                "Loading deprecated index.mantis.js — rename to main.mb.js.");
            evalFile(legacy_script.string());
            return;
        }

        m_app.logger().trace(
            "Scripting",
            fmt::format("No entry script found in `{}` (expected main.mb.js)", scripts_dir.string()));
    }

    void ScriptingEngine::loadScript(const std::string &relativePath) {
        if (!m_ctx) {
            return;
        }
        const fs::path script_path(relativePath);
        const auto full_path = script_path.is_absolute()
                                   ? script_path
                                   : fs::path(m_app.scriptsDir()) / relativePath;
        evalFile(full_path.string());
    }

    void ScriptingEngine::evalFile(const std::string &filePath) {
        if (!fs::exists(fs::path(filePath))) {
            m_app.logger().trace(
                "Scripting",
                fmt::format("Script file does not exist: `{}`", filePath));
            return;
        }

        std::ifstream file(filePath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        evalString(buffer.str(), filePath);
    }

    void ScriptingEngine::evalString(const std::string &content, const std::string &label) {
        if (!m_ctx) {
            return;
        }

        try {
            withLock([&] {
                dukglue_peval<void>(m_ctx, content.c_str());
            });
        } catch (const DukErrorException &e) {
            m_app.logger().critical(
                "Scripting",
                fmt::format("Error executing `{}`: {}", label, e.what()));
        }
    }

    void ScriptingEngine::fireOnServerStart() {
        if (!m_ctx) {
            return;
        }
        withLock([&] {
            ScriptingHooks::fireOnServerStart(m_ctx);
        });
    }

    void ScriptingEngine::fireOnRecordCreated(const std::string &entity, const std::string &recordId) {
        if (!m_ctx) {
            return;
        }
        withLock([&] {
            ScriptingHooks::fireOnRecordCreated(m_ctx, entity, recordId);
        });
    }

    void ScriptingEngine::fireOnRecordUpdated(const std::string &entity, const std::string &recordId) {
        if (!m_ctx) {
            return;
        }
        withLock([&] {
            ScriptingHooks::fireOnRecordUpdated(m_ctx, entity, recordId);
        });
    }

    bool ScriptingEngine::pcallBool(const DukValue &fn, MantisRequest &req, MantisResponse &res) {
        return withLock([&] -> bool {
            try {
                return dukglue_pcall<bool>(m_ctx, fn, &req, &res);
            } catch (const DukErrorException &e) {
                m_app.logger().critical("Scripting", fmt::format("JS middleware error: {}", e.what()));
                return false;
            }
        });
    }

    void ScriptingEngine::pcallVoid(const DukValue &fn, MantisRequest &req, MantisResponse &res) {
        withLock([&] {
            try {
                dukglue_pcall<void>(m_ctx, fn, &req, &res);
            } catch (const DukErrorException &e) {
                m_app.logger().critical("Scripting", fmt::format("JS handler error: {}", e.what()));
            }
        });
    }
}

#endif // MB_SCRIPTING_ENABLED
