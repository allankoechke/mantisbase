#include "request_wrap.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Napi::FunctionReference RequestWrap::s_constructor;

static Napi::Value JsonToNapi(Napi::Env env, const json& j) {
    if (j.is_null()) return env.Null();
    if (j.is_boolean()) return Napi::Boolean::New(env, j.get<bool>());
    if (j.is_number_integer()) return Napi::Number::New(env, j.get<int64_t>());
    if (j.is_number_float()) return Napi::Number::New(env, j.get<double>());
    if (j.is_string()) return Napi::String::New(env, j.get<std::string>());
    if (j.is_array()) {
        auto arr = Napi::Array::New(env, j.size());
        for (size_t i = 0; i < j.size(); ++i) {
            arr.Set(static_cast<uint32_t>(i), JsonToNapi(env, j[i]));
        }
        return arr;
    }
    if (j.is_object()) {
        auto obj = Napi::Object::New(env);
        for (auto it = j.begin(); it != j.end(); ++it) {
            obj.Set(it.key(), JsonToNapi(env, it.value()));
        }
        return obj;
    }
    return env.Undefined();
}

Napi::Function RequestWrap::GetClass(Napi::Env env) {
    auto func = DefineClass(env, "MantisRequest", {
        InstanceMethod<&RequestWrap::PathParam>("pathParam"),
        InstanceMethod<&RequestWrap::QueryParam>("queryParam"),
        InstanceMethod<&RequestWrap::Header>("header"),
        InstanceMethod<&RequestWrap::Json>("json"),
        InstanceMethod<&RequestWrap::Body>("body"),
        InstanceAccessor<&RequestWrap::GetMethod>("method"),
        InstanceAccessor<&RequestWrap::GetPath>("path"),
        InstanceAccessor<&RequestWrap::GetRemoteAddr>("remoteAddr"),
    });

    s_constructor = Napi::Persistent(func);
    s_constructor.SuppressDestruct();

    return func;
}

RequestWrap::RequestWrap(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<RequestWrap>(info) {

    if (info.Length() > 0 && info[0].IsExternal()) {
        m_req = info[0].As<Napi::External<mb::MantisRequest>>().Data();
    }
}

Napi::Object RequestWrap::NewInstance(Napi::Env env, mb::MantisRequest* req) {
    return s_constructor.New({Napi::External<mb::MantisRequest>::New(env, req)});
}

Napi::Value RequestWrap::PathParam(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected (name: string)").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();
    return Napi::String::New(env, m_req->getPathParamValue(name));
}

Napi::Value RequestWrap::QueryParam(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected (name: string)").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();
    return Napi::String::New(env, m_req->getQueryParamValue(name));
}

Napi::Value RequestWrap::Header(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected (name: string)").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();
    return Napi::String::New(env, m_req->getHeaderValue(name));
}

Napi::Value RequestWrap::Json(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    try {
        json j = m_req->jsonBody();
        return JsonToNapi(env, j);
    } catch (const std::exception& e) {
        Napi::Error::New(env, std::string("Failed to parse JSON body: ") + e.what())
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

Napi::Value RequestWrap::Body(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), m_req->getBody());
}

Napi::Value RequestWrap::GetMethod(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), m_req->getMethod());
}

Napi::Value RequestWrap::GetPath(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), m_req->getPath());
}

Napi::Value RequestWrap::GetRemoteAddr(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), m_req->getRemoteAddr());
}
