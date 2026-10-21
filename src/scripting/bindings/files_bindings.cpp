#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/files.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerFilesBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbFilesMgr::dirPath, "dirPath");
        dukglue_register_method(ctx, &MbFilesMgr::filePath, "filePath");
        dukglue_register_method(ctx, &MbFilesMgr::getFilePathString, "getFilePath");
        dukglue_register_method(ctx, &MbFilesMgr::removeFile, "removeFile");
    }
}

#endif // MB_SCRIPTING_ENABLED
