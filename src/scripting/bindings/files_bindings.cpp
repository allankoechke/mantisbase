#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/files.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerFilesBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbFilesMgr::createDir, "createDir");
        dukglue_register_method(ctx, &MbFilesMgr::renameDir, "renameDir");
        dukglue_register_method(ctx, &MbFilesMgr::deleteDir, "deleteDir");
        dukglue_register_method(ctx, &MbFilesMgr::dirPath, "dirPath");
        dukglue_register_method(ctx, &MbFilesMgr::filePath, "filePath");
        dukglue_register_method(ctx, &MbFilesMgr::getFilePathString, "getFilePath");
        dukglue_register_method(ctx, &MbFilesMgr::removeFile, "removeFile");
        dukglue_register_method(ctx, &MbFilesMgr::removeFilesJson, "removeFiles");
        dukglue_register_method(ctx, &MbFilesMgr::fileExists, "fileExists");
        dukglue_register_method(ctx, &MbFilesMgr::canonicalPathString, "canonicalPath");
        dukglue_register_method(ctx, &MbFilesMgr::isCanonicalPathString, "isCanonical");
        dukglue_register_method(ctx, &MbFilesMgr::filesBaseDirString, "baseDir");
    }
}

#endif // MB_SCRIPTING_ENABLED
