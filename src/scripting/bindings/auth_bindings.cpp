#include "../../../include/mantisbase/scripting/bindings.h"

#ifdef MB_SCRIPTING_ENABLED

#include "../../../include/mantisbase/core/api_keys.h"
#include "../../../include/mantisbase/core/auth.h"
#include "../../../include/mantisbase/core/oauth.h"

#include <dukglue/dukglue.h>

namespace mb {
    void registerAuthBindings(duk_context *ctx) {
        dukglue_register_method(ctx, &MbAuth::createTokenJson, "createToken");
        dukglue_register_method(ctx, &MbAuth::verifyTokenJson, "verifyToken");
        dukglue_register_method(ctx, &MbAuth::deleteSession, "deleteSession");
        dukglue_register_method(ctx, &MbAuth::refreshSessionJson, "refreshSession");
        dukglue_register_method(ctx, &MbAuth::sessionTimeoutSeconds, "sessionTimeoutSeconds");
        dukglue_register_method(ctx, &MbAuth::requestLoginOtpJson, "requestLoginOtp");
        dukglue_register_method(ctx, &MbAuth::loginWithOtpJson, "loginWithOtp");
        dukglue_register_method(ctx, &MbAuth::rejectUnverifiedLoginJson, "rejectUnverifiedLogin");
        dukglue_register_method(ctx, &MbAuth::notifyEmailVerification, "notifyEmailVerification");
        dukglue_register_method(ctx, &MbAuth::requestEmailVerificationJson, "requestEmailVerification");
        dukglue_register_method(ctx, &MbAuth::confirmEmailVerificationJson, "confirmEmailVerification");
        dukglue_register_method(ctx, &MbAuth::requestPasswordResetJson, "requestPasswordReset");
        dukglue_register_method(ctx, &MbAuth::confirmPasswordResetJson, "confirmPasswordReset");
        dukglue_register_method(ctx, &MbAuth::duk_apiKey, "apiKey");
        dukglue_register_method(ctx, &MbAuth::duk_oauth, "oauth");

        dukglue_register_method(ctx, &MbApiKeyMgr::createJson, "create");
        dukglue_register_method(ctx, &MbApiKeyMgr::listJson, "list");
        dukglue_register_method(ctx, &MbApiKeyMgr::revoke, "revoke");
        dukglue_register_method(ctx, &MbApiKeyMgr::revokeById, "revokeById");
        dukglue_register_method(ctx, &MbApiKeyMgr::listAdminJson, "listAdmin");
        dukglue_register_method(ctx, &MbApiKeyMgr::createAdminJson, "createAdmin");
        dukglue_register_method(ctx, &MbApiKeyMgr::revokeAdmin, "revokeAdmin");

        dukglue_register_method(ctx, &MbOAuthMgr::authorizeUrlJson, "authorizeUrl");
        dukglue_register_method(ctx, &MbOAuthMgr::handleCallbackJson, "handleCallback");
        dukglue_register_method(ctx, &MbOAuthMgr::linkAccountJson, "linkAccount");
        dukglue_register_method(ctx, &MbOAuthMgr::unlinkAccount, "unlinkAccount");
        dukglue_register_method(ctx, &MbOAuthMgr::linkedAccountsJson, "linkedAccounts");
        dukglue_register_method(ctx, &MbOAuthMgr::providersJson, "providers");
        dukglue_register_method(ctx, &MbOAuthMgr::addProviderJson, "addProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::updateProviderJson, "updateProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::removeProvider, "removeProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::listProvidersJson, "listProviders");
        dukglue_register_method(ctx, &MbOAuthMgr::enableProviderJson, "enableProvider");
        dukglue_register_method(ctx, &MbOAuthMgr::disableProviderForEntity, "disableProvider");
    }
}

#endif // MB_SCRIPTING_ENABLED
