#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/http.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRequestBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbRequest::hasHeader, "hasHeader");
        dukglue_register_method(ctx, &MbRequest::getHeaderValue, "getHeader");
        dukglue_register_method(ctx, &MbRequest::getHeaderValueU64, "getHeaderU64");
        dukglue_register_method(ctx, &MbRequest::getHeaderValueCount, "getHeaderCount");

        dukglue_register_method(ctx, &MbRequest::hasQueryParam, "hasQueryParam");
        dukglue_register_method(
            ctx,
            static_cast<std::string (MbRequest::*)(const std::string &) const>(
                &MbRequest::getQueryParamValue),
            "getQueryParam");
        dukglue_register_method(ctx, &MbRequest::getQueryParamValueCount, "getQueryParamCount");

        dukglue_register_method(ctx, &MbRequest::hasPathParam, "hasPathParam");
        dukglue_register_method(
            ctx,
            static_cast<std::string (MbRequest::*)(const std::string &) const>(
                &MbRequest::getPathParamValue),
            "getPathParam");
        dukglue_register_method(ctx, &MbRequest::getPathParamValueCount, "getPathParamCount");

        dukglue_register_method(ctx, &MbRequest::isMultipartFormData, "isMultipartFormData");

        dukglue_register_property(ctx, &MbRequest::getBody, nullptr, "body");
        dukglue_register_property(ctx, &MbRequest::getMethod, nullptr, "method");
        dukglue_register_property(ctx, &MbRequest::getPath, nullptr, "path");
        dukglue_register_property(ctx, &MbRequest::getRemoteAddr, nullptr, "remoteAddr");
        dukglue_register_property(ctx, &MbRequest::getRemotePort, nullptr, "remotePort");
        dukglue_register_property(ctx, &MbRequest::getLocalAddr, nullptr, "localAddr");
        dukglue_register_property(ctx, &MbRequest::getLocalPort, nullptr, "localPort");

        dukglue_register_method(ctx, &MbRequest::hasKey, "hasKey");
        dukglue_register_method(ctx, &MbRequest::set_duk, "set");
        dukglue_register_method(ctx, &MbRequest::get_duk, "get");
        dukglue_register_method(ctx, &MbRequest::getOr_duk, "getOr");
    }
}

#endif // MB_SCRIPTING_ENABLED
