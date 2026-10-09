#pragma once

#include <napi.h>
#include <mantisbase/core/database.h>

class DbWrap : public Napi::ObjectWrap<DbWrap> {
public:
    static Napi::Function GetClass(Napi::Env env);
    static Napi::Object NewInstance(Napi::Env env, mb::Database* db);
    DbWrap(const Napi::CallbackInfo& info);

private:
    Napi::Value Query(const Napi::CallbackInfo& info);
    Napi::Value IsConnected(const Napi::CallbackInfo& info);

    mb::Database* m_db = nullptr;
    static Napi::FunctionReference s_constructor;
};
