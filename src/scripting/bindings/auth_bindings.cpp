#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/api_keys.h"
#include "../../../include/mantisbase/core/auth.h"
#include "../../../include/mantisbase/core/oauth.h"

#include <dukglue/dukglue.h>
#include "../../../include/mantisbase/scripting/duk_json.h"

namespace mb {
    void registerAuthBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbAuth::createTokenJson, "createToken");
        dukglue_register_method(ctx, &MbAuth::verifyTokenJson, "verifyToken");
        dukglue_register_method(ctx, &MbAuth::deleteSession, "deleteSession");
        dukglue_register_method(ctx, &MbAuth::refreshSessionJson, "refreshSession");
        dukglue_register_method(ctx, &MbAuth::sessionTimeoutSeconds, "sessionTimeoutSeconds");
        dukglue_register_method(ctx, &MbAuth::requestLoginOtp, "requestLoginOtp");
        dukglue_register_method(ctx, &MbAuth::loginWithOtp, "loginWithOtp");
        dukglue_register_method(ctx, &MbAuth::rejectUnverifiedLogin, "rejectUnverifiedLogin");
        dukglue_register_method(ctx, &MbAuth::notifyEmailVerification, "notifyEmailVerification");
        dukglue_register_method(ctx, &MbAuth::requestEmailVerification, "requestEmailVerification");
        dukglue_register_method(ctx, &MbAuth::confirmEmailVerification, "confirmEmailVerification");
        dukglue_register_method(ctx, &MbAuth::requestPasswordReset, "requestPasswordReset");
        dukglue_register_method(ctx, &MbAuth::confirmPasswordReset, "confirmPasswordReset");
        dukglue_register_method(ctx, &MbAuth::duk_apiKey, "apiKey");
        dukglue_register_method(ctx, &MbAuth::duk_oauth, "oauth");

        dukglue_register_method(ctx, &MbApiKeyMgr::createJson, "create");
        dukglue_register_method(ctx, &MbApiKeyMgr::list, "list");
        dukglue_register_method(ctx, &MbApiKeyMgr::revoke, "revoke");
        dukglue_register_method(ctx, &MbApiKeyMgr::revokeById, "revokeById");
        dukglue_register_method(ctx, &MbApiKeyMgr::listAdmin, "listAdmin");
        dukglue_register_method(ctx, &MbApiKeyMgr::createAdminJson, "createAdmin");
        dukglue_register_method(ctx, &MbApiKeyMgr::revokeAdmin, "revokeAdmin");

        dukglue_register_method(ctx, &MbOAuthMgr::buildAuthorizeUrl, "authorizeUrl");
        dukglue_register_method(ctx, &MbOAuthMgr::handleCallback, "handleCallback");
        dukglue_register_method(ctx, &MbOAuthMgr::linkAccount, "linkAccount");
        dukglue_register_method(ctx, &MbOAuthMgr::unlinkAccount, "unlinkAccount");
        dukglue_register_method(ctx, &MbOAuthMgr::getLinkedAccounts, "linkedAccounts");
        dukglue_register_method(ctx, &MbOAuthMgr::getProviders, "providers");
        dukglue_register_method(ctx, &MbOAuthMgr::addProvider, "addProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::updateProvider, "updateProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::removeProvider, "removeProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::listProviders, "listProviders");
        dukglue_register_method(ctx, &MbOAuthMgr::enableProviderForEntity, "enableProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::disableProviderForEntity, "disableProvider");
    }
}

#endif // MB_SCRIPTING_ENABLED
