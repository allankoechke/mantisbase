#include "app_wrap.h"
#include "router_wrap.h"
#include "db_wrap.h"
#include <nlohmann/json.hpp>
#include <cstdlib>

using json = nlohmann::json;

Napi::Function AppWrap::GetClass(Napi::Env env) {
    return DefineClass(env, "App", {
        InstanceMethod<&AppWrap::Start>("start"),
        InstanceMethod<&AppWrap::Stop>("stop"),
        InstanceAccessor<&AppWrap::GetRouter>("router"),
        InstanceAccessor<&AppWrap::GetDb>("db"),
    });
}

AppWrap::AppWrap(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<AppWrap>(info) {

    json config = json::object();
    config["serve"] = json::object();

    if (info.Length() > 0 && info[0].IsObject()) {
        Napi::Object opts = info[0].As<Napi::Object>();

        if (opts.Has("port") && opts.Get("port").IsNumber()) {
            config["serve"]["port"] = opts.Get("port").As<Napi::Number>().Int32Value();
        }
        if (opts.Has("host") && opts.Get("host").IsString()) {
            config["serve"]["host"] = opts.Get("host").As<Napi::String>().Utf8Value();
        }
        if (opts.Has("dataDir") && opts.Get("dataDir").IsString()) {
            config["data-dir"] = opts.Get("dataDir").As<Napi::String>().Utf8Value();
        }
        if (opts.Has("dbType") && opts.Get("dbType").IsString()) {
            config["db"] = opts.Get("dbType").As<Napi::String>().Utf8Value();
        }
        if (opts.Has("dbUrl") && opts.Get("dbUrl").IsString()) {
            config["db_url"] = opts.Get("dbUrl").As<Napi::String>().Utf8Value();
        }
        if (opts.Has("publicDir") && opts.Get("publicDir").IsString()) {
            config["public-dir"] = opts.Get("publicDir").As<Napi::String>().Utf8Value();
        }
        if (opts.Has("scriptsDir") && opts.Get("scriptsDir").IsString()) {
            config["scripts-dir"] = opts.Get("scriptsDir").As<Napi::String>().Utf8Value();
        }
        if (opts.Has("migrationsDir") && opts.Get("migrationsDir").IsString()) {
            config["migrations-dir"] = opts.Get("migrationsDir").As<Napi::String>().Utf8Value();
        }
        if (opts.Has("poolSize") && opts.Get("poolSize").IsNumber()) {
            config["serve"]["pool-size"] = opts.Get("poolSize").As<Napi::Number>().Int32Value();
        }
        // `dev` is a presence-only flag in MantisBase::create() -- only add the
        // key when the caller actually asked for dev mode.
        if (opts.Has("dev") && opts.Get("dev").ToBoolean().Value()) {
            config["dev"] = true;
        }
        if (opts.Has("skipAdminSetup") && opts.Get("skipAdminSetup").IsBoolean()) {
            config["serve"]["skip-admin-setup"] = opts.Get("skipAdminSetup").As<Napi::Boolean>().Value();
        }

        // MantisBase reads the JWT signing key from the environment, so there is
        // no CLI/config equivalent to forward it through.
        if (opts.Has("secretKey") && opts.Get("secretKey").IsString()) {
            const std::string secret = opts.Get("secretKey").As<Napi::String>().Utf8Value();
#ifdef _WIN32
            _putenv_s("MB_JWT_SECRET", secret.c_str());
#else
            ::setenv("MB_JWT_SECRET", secret.c_str(), 1);
#endif
        }
    }

    m_app = mb::MantisBase::create(config);
    if (!m_app) {
        Napi::Error::New(info.Env(), "Failed to create MantisBase instance")
            .ThrowAsJavaScriptException();
    }
}

AppWrap::~AppWrap() {
    if (m_running.load()) {
        m_app->close();
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }
}

Napi::Value AppWrap::Start(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (m_running.load()) {
        return env.Undefined();
    }

    m_running.store(true);
    m_serverThread = std::thread([this]() {
        m_app->run();
        m_running.store(false);
    });

    return env.Undefined();
}

Napi::Value AppWrap::Stop(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!m_running.load()) {
        return env.Undefined();
    }

    m_app->close();
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    m_running.store(false);

    return env.Undefined();
}

Napi::Value AppWrap::GetRouter(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (m_routerRef.IsEmpty()) {
        Napi::Object routerObj = RouterWrap::NewInstance(env, &m_app->router());
        m_routerRef = Napi::Persistent(routerObj);
        m_routerRef.SuppressDestruct();
    }

    return m_routerRef.Value();
}

Napi::Value AppWrap::GetDb(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (m_dbRef.IsEmpty()) {
        Napi::Object dbObj = DbWrap::NewInstance(env, &m_app->db());
        m_dbRef = Napi::Persistent(dbObj);
        m_dbRef.SuppressDestruct();
    }

    return m_dbRef.Value();
}
