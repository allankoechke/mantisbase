#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/http.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerRequestBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MantisRequest::hasHeader, "hasHeader");
        dukglue_register_method(ctx, &MantisRequest::getHeaderValue, "getHeader");
        dukglue_register_method(ctx, &MantisRequest::getHeaderValueU64, "getHeaderU64");
        dukglue_register_method(ctx, &MantisRequest::getHeaderValueCount, "getHeaderCount");

        dukglue_register_method(ctx, &MantisRequest::hasQueryParam, "hasQueryParam");
        dukglue_register_method(
            ctx,
            static_cast<std::string (MantisRequest::*)(const std::string &) const>(
                &MantisRequest::getQueryParamValue),
            "getQueryParam");
        dukglue_register_method(ctx, &MantisRequest::getQueryParamValueCount, "getQueryParamCount");

        dukglue_register_method(ctx, &MantisRequest::hasPathParam, "hasPathParam");
        dukglue_register_method(
            ctx,
            static_cast<std::string (MantisRequest::*)(const std::string &) const>(
                &MantisRequest::getPathParamValue),
            "getPathParam");
        dukglue_register_method(ctx, &MantisRequest::getPathParamValueCount, "getPathParamCount");

        dukglue_register_method(ctx, &MantisRequest::isMultipartFormData, "isMultipartFormData");

        dukglue_register_property(ctx, &MantisRequest::getBody, nullptr, "body");
        dukglue_register_property(ctx, &MantisRequest::getMethod, nullptr, "method");
        dukglue_register_property(ctx, &MantisRequest::getPath, nullptr, "path");
        dukglue_register_property(ctx, &MantisRequest::getRemoteAddr, nullptr, "remoteAddr");
        dukglue_register_property(ctx, &MantisRequest::getRemotePort, nullptr, "remotePort");
        dukglue_register_property(ctx, &MantisRequest::getLocalAddr, nullptr, "localAddr");
        dukglue_register_property(ctx, &MantisRequest::getLocalPort, nullptr, "localPort");

        dukglue_register_method(ctx, &MantisRequest::hasKey, "hasKey");
        dukglue_register_method(ctx, &MantisRequest::set_duk, "set");
        dukglue_register_method(ctx, &MantisRequest::get_duk, "get");
        dukglue_register_method(ctx, &MantisRequest::getOr_duk, "getOr");
    }
}

#endif // MB_SCRIPTING_ENABLED
