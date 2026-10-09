#pragma once

#include <napi.h>
#include <mantisbase/core/http.h>

class ResponseWrap : public Napi::ObjectWrap<ResponseWrap> {
public:
    static Napi::Function GetClass(Napi::Env env);
    ResponseWrap(const Napi::CallbackInfo& info);

    static Napi::Object NewInstance(Napi::Env env, mb::MantisResponse* res);

private:
    Napi::Value JsonMethod(const Napi::CallbackInfo& info);
    Napi::Value Html(const Napi::CallbackInfo& info);
    Napi::Value Text(const Napi::CallbackInfo& info);
    Napi::Value Send(const Napi::CallbackInfo& info);
    Napi::Value Redirect(const Napi::CallbackInfo& info);
    Napi::Value SetHeader(const Napi::CallbackInfo& info);

    mb::MantisResponse* m_res = nullptr;

    static Napi::FunctionReference s_constructor;
};
