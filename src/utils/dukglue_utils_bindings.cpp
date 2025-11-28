#include "../../include/mantis/utils/utils.h"
#include "../../include/mantis/app/app.h"

namespace mantis {
    void registerUtilsToDuktapeEngine()
    {
        const auto ctx = MantisBase::instance().ctx();

        dukglue_register_function(ctx, &generateTimeBasedId, "generateTimeBasedId");
        dukglue_register_function(ctx, &generateReadableTimeId, "generateReadableTimeId");
        dukglue_register_function(ctx, &generateShortId, "generateShortId");
        dukglue_register_function(ctx, &getEnvOrDefault, "getEnvOrDefault");
        dukglue_register_function(ctx, &sanitizeFilename_JSWrapper, "sanitizeFilename");
        dukglue_register_function(ctx, &hashPassword, "hashPassword");
        dukglue_register_function(ctx, &verifyPassword, "verifyPassword");
    }
}