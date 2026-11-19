#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/email.h"

#include <dukglue/dukglue.h>
#include "../../../include/mantisbase/scripting/duk_json.h"

namespace mb {
    void registerMailerBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbMailer::isConfigured, "isConfigured");
        dukglue_register_method(ctx, &MbMailer::configJson, "config");
        dukglue_register_method(ctx, &MbMailer::configureJson, "configure");
        dukglue_register_method(ctx, &MbMailer::applySettingsScript, "applySettings");
        dukglue_register_method(ctx, &MbMailer::sendMessage, "send");
        dukglue_register_method(ctx, &MbMailer::sendTest, "sendTest");
        dukglue_register_method(ctx, &MbMailer::loadTemplateJson, "loadTemplate");
        dukglue_register_method(ctx, &MbMailer::checkRateLimit, "checkRateLimit");
        dukglue_register_method(ctx, &MbMailer::sentMessagesJson, "sentMessages");
        dukglue_register_method(ctx, &MbMailer::clearSentMessages, "clearSentMessages");
        dukglue_register_method(ctx, &MbMailer::clearRateLimits, "clearRateLimits");
    }
}

#endif // MB_SCRIPTING_ENABLED
