#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/utils/utils.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerUtilsBindings(duk_context *ctx) {
        dukglue_register_function(ctx, &generateTimeBasedId, "generateTimeBasedId");
        dukglue_register_function(ctx, &generateReadableTimeId, "generateReadableTimeId");
        dukglue_register_function(ctx, &generateShortId, "generateShortId");
        dukglue_register_function(ctx, &getEnvOrDefault, "getEnvOrDefault");
        dukglue_register_function(ctx, &sanitizeFilename_JSWrapper, "sanitizeFilename");
        dukglue_register_function(ctx, &hashPassword, "hashPassword");
        dukglue_register_function(ctx, &verifyPassword, "verifyPassword");

        duk_peval_string(ctx,
                         "utils = {"
                         "  generateTimeBasedId: generateTimeBasedId,"
                         "  generateReadableTimeId: generateReadableTimeId,"
                         "  generateShortId: generateShortId,"
                         "  getEnvOrDefault: getEnvOrDefault,"
                         "  sanitizeFilename: sanitizeFilename,"
                         "  hashPassword: hashPassword,"
                         "  verifyPassword: verifyPassword"
                         "};");
    }
}

#endif // MB_SCRIPTING_ENABLED
