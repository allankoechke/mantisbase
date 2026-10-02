#pragma once

#include <napi.h>
#include <mantisbase/mantisbase.h>
#include <thread>
#include <atomic>

class RouterWrap;
class DbWrap;

class AppWrap : public Napi::ObjectWrap<AppWrap> {
public:
    static Napi::Function GetClass(Napi::Env env);
    AppWrap(const Napi::CallbackInfo& info);
    ~AppWrap();

    mb::MantisBase* getApp() { return m_app.get(); }

private:
    Napi::Value Start(const Napi::CallbackInfo& info);
    Napi::Value Stop(const Napi::CallbackInfo& info);
    Napi::Value GetRouter(const Napi::CallbackInfo& info);
    Napi::Value GetDb(const Napi::CallbackInfo& info);

    std::unique_ptr<mb::MantisBase> m_app;
    std::thread m_serverThread;
    std::atomic<bool> m_running{false};
    Napi::ObjectReference m_routerRef;
    Napi::ObjectReference m_dbRef;
};
