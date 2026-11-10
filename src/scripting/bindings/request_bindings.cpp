#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/http.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRequestBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbRequest::hasHeader, "hasHeader");
        dukglue_register_method(ctx, &MbRequest::headerValue, "getHeader");

        dukglue_register_method(ctx, &MbRequest::hasQueryParam, "hasQueryParam");
        dukglue_register_method(
            ctx,
            static_cast<std::string (MbRequest::*)(const std::string &) const>(
                &MbRequest::queryParamValue),
            "getQueryParam");

        dukglue_register_method(ctx, &MbRequest::hasPathParam, "hasPathParam");
        dukglue_register_method(
            ctx,
            static_cast<std::string (MbRequest::*)(const std::string &) const>(
                &MbRequest::pathParamValue),
            "getPathParam");

        dukglue_register_method(ctx, &MbRequest::isMultipartFormData, "isMultipartFormData");

        dukglue_register_property(ctx, &MbRequest::body, nullptr, "body");
        dukglue_register_property(ctx, &MbRequest::method, nullptr, "method");
        dukglue_register_property(ctx, &MbRequest::path, nullptr, "path");
        dukglue_register_property(ctx, &MbRequest::remoteAddr, nullptr, "remoteAddr");
        dukglue_register_property(ctx, &MbRequest::remotePort, nullptr, "remotePort");
        dukglue_register_property(ctx, &MbRequest::localAddr, nullptr, "localAddr");
        dukglue_register_property(ctx, &MbRequest::localPort, nullptr, "localPort");

        dukglue_register_method(ctx, &MbRequest::hasKey, "hasKey");
        dukglue_register_method(ctx, &MbRequest::set_duk, "set");
        dukglue_register_method(ctx, &MbRequest::get_duk, "get");
        dukglue_register_method(ctx, &MbRequest::getOr_duk, "getOr");
    }
}

#endif // MB_SCRIPTING_ENABLED
