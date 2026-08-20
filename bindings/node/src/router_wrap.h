#pragma once

#include <napi.h>
#include <mantisbase/core/router.h>
#include <mantisbase/core/types.h>
#include <vector>

class RouterWrap : public Napi::ObjectWrap<RouterWrap> {
public:
    static Napi::Function GetClass(Napi::Env env);
    RouterWrap(const Napi::CallbackInfo& info);
    ~RouterWrap();

    void SetRouter(mb::Router* router) { m_router = router; }

    static Napi::Object NewInstance(Napi::Env env, mb::Router* router);

private:
    Napi::Value Get(const Napi::CallbackInfo& info);
    Napi::Value Post(const Napi::CallbackInfo& info);
    Napi::Value Patch(const Napi::CallbackInfo& info);
    Napi::Value Delete(const Napi::CallbackInfo& info);

    void RegisterRoute(const Napi::CallbackInfo& info,
        void(mb::Router::*method)(const std::string&, const mb::HandlerFn&, const mb::Middlewares&));

    mb::Router* m_router = nullptr;
    std::vector<Napi::ThreadSafeFunction> m_tsfns;

    static Napi::FunctionReference s_constructor;
};
