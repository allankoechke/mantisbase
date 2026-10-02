#include "router_wrap.h"
#include "request_wrap.h"
#include "response_wrap.h"
#include <future>
#include <memory>

Napi::FunctionReference RouterWrap::s_constructor;

Napi::Function RouterWrap::GetClass(Napi::Env env) {
    auto func = DefineClass(env, "Router", {
        InstanceMethod<&RouterWrap::Get>("get"),
        InstanceMethod<&RouterWrap::Post>("post"),
        InstanceMethod<&RouterWrap::Patch>("patch"),
        InstanceMethod<&RouterWrap::Delete>("delete"),
    });

    s_constructor = Napi::Persistent(func);
    s_constructor.SuppressDestruct();

    return func;
}

RouterWrap::RouterWrap(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<RouterWrap>(info) {

    if (info.Length() > 0 && info[0].IsExternal()) {
        m_router = info[0].As<Napi::External<mb::Router>>().Data();
    }
}

RouterWrap::~RouterWrap() {
    for (auto& tsfn : m_tsfns) {
        tsfn.Release();
    }
    m_tsfns.clear();
}

Napi::Object RouterWrap::NewInstance(Napi::Env env, mb::Router* router) {
    Napi::Object obj = s_constructor.New({Napi::External<mb::Router>::New(env, router)});
    return obj;
}

void RouterWrap::RegisterRoute(const Napi::CallbackInfo& info,
    void(mb::Router::*method)(const std::string&, const mb::HandlerFn&, const mb::Middlewares&)) {

    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected (path: string, handler: function)")
            .ThrowAsJavaScriptException();
        return;
    }

    if (!m_router) {
        Napi::Error::New(env, "Router not initialized").ThrowAsJavaScriptException();
        return;
    }

    std::string path = info[0].As<Napi::String>().Utf8Value();
    Napi::Function handler = info[1].As<Napi::Function>();

    auto tsfn = Napi::ThreadSafeFunction::New(
        env, handler, "MantisRouteHandler", 0, 1);

    m_tsfns.push_back(tsfn);

    mb::HandlerFn handlerFn = [tsfn](mb::MantisRequest& req, mb::MantisResponse& res) mutable {
        auto donePtr = std::make_shared<std::promise<void>>();
        auto future = donePtr->get_future();

        auto reqPtr = &req;
        auto resPtr = &res;

        tsfn.BlockingCall(
            [reqPtr, resPtr, donePtr](Napi::Env env, Napi::Function jsCallback) {
                Napi::Object reqObj = RequestWrap::NewInstance(env, reqPtr);
                Napi::Object resObj = ResponseWrap::NewInstance(env, resPtr);

                Napi::Value result = jsCallback.Call({reqObj, resObj});

                if (env.IsExceptionPending()) {
                    donePtr->set_value();
                    return;
                }

                if (result.IsPromise()) {
                    Napi::Object promise = result.As<Napi::Object>();

                    auto thenFn = Napi::Function::New(env,
                        [donePtr](const Napi::CallbackInfo& cbInfo) {
                            donePtr->set_value();
                        });

                    auto catchFn = Napi::Function::New(env,
                        [donePtr](const Napi::CallbackInfo& cbInfo) {
                            donePtr->set_value();
                        });

                    Napi::Value thenResult = promise.Get("then")
                        .As<Napi::Function>()
                        .Call(promise, {thenFn});

                    if (!env.IsExceptionPending() && thenResult.IsObject()) {
                        thenResult.As<Napi::Object>()
                            .Get("catch")
                            .As<Napi::Function>()
                            .Call(thenResult.As<Napi::Object>(), {catchFn});
                    }

                    if (env.IsExceptionPending()) {
                        donePtr->set_value();
                    }
                } else {
                    donePtr->set_value();
                }
            });

        future.wait();
    };

    (m_router->*method)(path, handlerFn, {});
}

Napi::Value RouterWrap::Get(const Napi::CallbackInfo& info) {
    RegisterRoute(info, &mb::Router::Get);
    return info.Env().Undefined();
}

Napi::Value RouterWrap::Post(const Napi::CallbackInfo& info) {
    RegisterRoute(info, static_cast<void(mb::Router::*)(const std::string&, const mb::HandlerFn&, const mb::Middlewares&)>(&mb::Router::Post));
    return info.Env().Undefined();
}

Napi::Value RouterWrap::Patch(const Napi::CallbackInfo& info) {
    RegisterRoute(info, static_cast<void(mb::Router::*)(const std::string&, const mb::HandlerFn&, const mb::Middlewares&)>(&mb::Router::Patch));
    return info.Env().Undefined();
}

Napi::Value RouterWrap::Delete(const Napi::CallbackInfo& info) {
    RegisterRoute(info, &mb::Router::Delete);
    return info.Env().Undefined();
}
