#include <napi.h>
#include "app_wrap.h"
#include "router_wrap.h"
#include "request_wrap.h"
#include "response_wrap.h"
#include "db_wrap.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("App", AppWrap::GetClass(env));
    exports.Set("MbRouter", RouterWrap::GetClass(env));
    exports.Set("MbRequest", RequestWrap::GetClass(env));
    exports.Set("MbResponse", ResponseWrap::GetClass(env));
    exports.Set("MbDatabase", DbWrap::GetClass(env));
    return exports;
}

NODE_API_MODULE(mantisbase, Init)
