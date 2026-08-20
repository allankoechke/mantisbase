#include <napi.h>
#include "app_wrap.h"
#include "router_wrap.h"
#include "request_wrap.h"
#include "response_wrap.h"
#include "db_wrap.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("App", AppWrap::GetClass(env));
    exports.Set("Router", RouterWrap::GetClass(env));
    exports.Set("MantisRequest", RequestWrap::GetClass(env));
    exports.Set("MantisResponse", ResponseWrap::GetClass(env));
    exports.Set("Database", DbWrap::GetClass(env));
    return exports;
}

NODE_API_MODULE(mantisbase, Init)
