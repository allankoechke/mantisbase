#pragma once

#include <napi.h>
#include <mantisbase/core/http.h>

class RequestWrap : public Napi::ObjectWrap<RequestWrap> {
public:
    static Napi::Function GetClass(Napi::Env env);
    RequestWrap(const Napi::CallbackInfo& info);

    static Napi::Object NewInstance(Napi::Env env, mb::MantisRequest* req);

private:
    Napi::Value PathParam(const Napi::CallbackInfo& info);
    Napi::Value QueryParam(const Napi::CallbackInfo& info);
    Napi::Value Header(const Napi::CallbackInfo& info);
    Napi::Value Json(const Napi::CallbackInfo& info);
    Napi::Value Body(const Napi::CallbackInfo& info);
    Napi::Value GetMethod(const Napi::CallbackInfo& info);
    Napi::Value GetPath(const Napi::CallbackInfo& info);
    Napi::Value GetRemoteAddr(const Napi::CallbackInfo& info);

    mb::MantisRequest* m_req = nullptr;

    static Napi::FunctionReference s_constructor;
};
