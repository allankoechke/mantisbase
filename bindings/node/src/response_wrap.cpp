#include "response_wrap.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Napi::FunctionReference ResponseWrap::s_constructor;

static json NapiToJson(Napi::Value val) {
    if (val.IsNull() || val.IsUndefined()) return nullptr;
    if (val.IsBoolean()) return val.As<Napi::Boolean>().Value();
    if (val.IsNumber()) {
        double d = val.As<Napi::Number>().DoubleValue();
        if (d == static_cast<int64_t>(d) && d >= -9007199254740992.0 && d <= 9007199254740992.0) {
            return static_cast<int64_t>(d);
        }
        return d;
    }
    if (val.IsString()) return val.As<Napi::String>().Utf8Value();
    if (val.IsArray()) {
        auto arr = val.As<Napi::Array>();
        json j = json::array();
        for (uint32_t i = 0; i < arr.Length(); ++i) {
            j.push_back(NapiToJson(arr.Get(i)));
        }
        return j;
    }
    if (val.IsObject()) {
        auto obj = val.As<Napi::Object>();
        json j = json::object();
        auto names = obj.GetPropertyNames();
        for (uint32_t i = 0; i < names.Length(); ++i) {
            std::string key = names.Get(i).As<Napi::String>().Utf8Value();
            j[key] = NapiToJson(obj.Get(key));
        }
        return j;
    }
    return nullptr;
}

Napi::Function ResponseWrap::GetClass(Napi::Env env) {
    auto func = DefineClass(env, "MantisResponse", {
        InstanceMethod<&ResponseWrap::JsonMethod>("json"),
        InstanceMethod<&ResponseWrap::Html>("html"),
        InstanceMethod<&ResponseWrap::Text>("text"),
        InstanceMethod<&ResponseWrap::Send>("send"),
        InstanceMethod<&ResponseWrap::Redirect>("redirect"),
        InstanceMethod<&ResponseWrap::SetHeader>("setHeader"),
    });

    s_constructor = Napi::Persistent(func);
    s_constructor.SuppressDestruct();

    return func;
}

ResponseWrap::ResponseWrap(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<ResponseWrap>(info) {

    if (info.Length() > 0 && info[0].IsExternal()) {
        m_res = info[0].As<Napi::External<mb::MantisResponse>>().Data();
    }
}

Napi::Object ResponseWrap::NewInstance(Napi::Env env, mb::MantisResponse* res) {
    return s_constructor.New({Napi::External<mb::MantisResponse>::New(env, res)});
}

Napi::Value ResponseWrap::JsonMethod(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected (statusCode: number, data: object)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    int statusCode = info[0].As<Napi::Number>().Int32Value();
    json data = NapiToJson(info[1]);
    m_res->sendJSON(statusCode, data);
    return env.Undefined();
}

Napi::Value ResponseWrap::Html(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected (statusCode: number, body: string)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    int statusCode = info[0].As<Napi::Number>().Int32Value();
    std::string body = info[1].As<Napi::String>().Utf8Value();
    m_res->sendHtml(statusCode, body);
    return env.Undefined();
}

Napi::Value ResponseWrap::Text(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected (statusCode: number, body: string)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    int statusCode = info[0].As<Napi::Number>().Int32Value();
    std::string body = info[1].As<Napi::String>().Utf8Value();
    m_res->sendText(statusCode, body);
    return env.Undefined();
}

Napi::Value ResponseWrap::Send(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected (statusCode: number, body?: string, contentType?: string)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    int statusCode = info[0].As<Napi::Number>().Int32Value();
    std::string body = (info.Length() > 1 && info[1].IsString())
        ? info[1].As<Napi::String>().Utf8Value() : "";
    std::string contentType = (info.Length() > 2 && info[2].IsString())
        ? info[2].As<Napi::String>().Utf8Value() : "text/plain";
    m_res->send(statusCode, body, contentType);
    return env.Undefined();
}

Napi::Value ResponseWrap::Redirect(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected (url: string)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string url = info[0].As<Napi::String>().Utf8Value();
    int status = (info.Length() > 1 && info[1].IsNumber())
        ? info[1].As<Napi::Number>().Int32Value() : 302;
    m_res->setRedirect(url, status);
    return env.Undefined();
}

Napi::Value ResponseWrap::SetHeader(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected (name: string, value: string)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string name = info[0].As<Napi::String>().Utf8Value();
    std::string value = info[1].As<Napi::String>().Utf8Value();
    m_res->setHeader(name, value);
    return env.Undefined();
}
