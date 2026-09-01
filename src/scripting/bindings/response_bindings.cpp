#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/http.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerResponseBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MantisResponse::hasHeader, "hasHeader");
        dukglue_register_method(ctx, &MantisResponse::getHeaderValue, "getHeader");
        dukglue_register_method(ctx, &MantisResponse::getHeaderValueU64, "getHeaderU64");
        dukglue_register_method(ctx, &MantisResponse::getHeaderValueCount, "getHeaderCount");
        dukglue_register_method(ctx, &MantisResponse::setHeader, "setHeader");

        dukglue_register_method(ctx, &MantisResponse::setRedirect, "redirect");

        dukglue_register_method(
            ctx,
            static_cast<void (MantisResponse::*)(const std::string &, const std::string &) const>(
                &MantisResponse::setContent),
            "setContent");
        dukglue_register_method(
            ctx,
            static_cast<void (MantisResponse::*)(const std::string &) const>(&MantisResponse::setFileContent),
            "setFileContent");

        dukglue_register_method(ctx, &MantisResponse::send, "send");
        dukglue_register_method(
            ctx,
            static_cast<void (MantisResponse::*)(int, const DukValue &) const>(&MantisResponse::sendJson),
            "json");
        dukglue_register_method(ctx, &MantisResponse::sendHtml, "html");
        dukglue_register_method(ctx, &MantisResponse::sendText, "text");
        dukglue_register_method(ctx, &MantisResponse::sendEmpty, "empty");

        dukglue_register_property(ctx, &MantisResponse::getBody, &MantisResponse::setBody, "body");
    }
}

#endif // MB_SCRIPTING_ENABLED
