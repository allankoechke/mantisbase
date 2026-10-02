#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/http.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerResponseBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbResponse::hasHeader, "hasHeader");
        dukglue_register_method(ctx, &MbResponse::getHeaderValue, "getHeader");
        dukglue_register_method(ctx, &MbResponse::getHeaderValueU64, "getHeaderU64");
        dukglue_register_method(ctx, &MbResponse::getHeaderValueCount, "getHeaderCount");
        dukglue_register_method(ctx, &MbResponse::setHeader, "setHeader");

        dukglue_register_method(ctx, &MbResponse::setRedirect, "redirect");

        dukglue_register_method(
            ctx,
            static_cast<void (MbResponse::*)(const std::string &, const std::string &) const>(
                &MbResponse::setContent),
            "setContent");
        dukglue_register_method(
            ctx,
            static_cast<void (MbResponse::*)(const std::string &) const>(&MbResponse::setFileContent),
            "setFileContent");

        dukglue_register_method(ctx, &MbResponse::send, "send");
        dukglue_register_method(
            ctx,
            static_cast<void (MbResponse::*)(int, const DukValue &) const>(&MbResponse::sendJson),
            "json");
        dukglue_register_method(ctx, &MbResponse::sendHtml, "html");
        dukglue_register_method(ctx, &MbResponse::sendText, "text");
        dukglue_register_method(ctx, &MbResponse::sendEmpty, "empty");

        dukglue_register_property(ctx, &MbResponse::getBody, &MbResponse::setBody, "body");
    }
}

#endif // MB_SCRIPTING_ENABLED
