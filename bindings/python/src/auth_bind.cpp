#include <mantisbase/core/auth.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "py_conv.h"

namespace nb = nanobind;

namespace {
    nlohmann::json permissions_of(nb::handle permissions) {
        if (!permissions.is_valid() || permissions.is_none())
            return nlohmann::json::array();
        nlohmann::json value = mbpy::python_to_json(permissions);
        if (!value.is_array())
            throw nb::type_error("permissions must be a list");
        return value;
    }
} // namespace

void register_auth(nb::module_ &m) {
    nb::class_<mb::MbApiKeyMgr>(m, "ApiKeys", "Create, list, and revoke API keys")
            .def(
                "create",
                [](const mb::MbApiKeyMgr &self, const std::string &entity,
                   const std::string &user_id, const std::string &label,
                   nb::handle permissions, const std::string &expires_at) {
                    const nlohmann::json perms = permissions_of(permissions);
                    return mbpy::call_json([&] {
                        return self.create(entity, user_id, label, perms, expires_at);
                    });
                },
                nb::arg("entity"), nb::arg("user_id"), nb::arg("label"),
                nb::arg("permissions") = nb::none(), nb::arg("expires_at") = "")
            .def(
                "list",
                [](const mb::MbApiKeyMgr &self, const std::string &entity,
                   const std::string &user_id) {
                    return mbpy::call_json([&] { return self.list(entity, user_id); });
                },
                nb::arg("entity"), nb::arg("user_id"))
            .def(
                "revoke",
                [](const mb::MbApiKeyMgr &self, const std::string &key_id,
                   const std::string &entity, const std::string &user_id) {
                    nb::gil_scoped_release release;
                    return self.revoke(key_id, entity, user_id);
                },
                nb::arg("key_id"), nb::arg("entity"), nb::arg("user_id"))
            .def(
                "revoke_by_id",
                [](const mb::MbApiKeyMgr &self, const std::string &key_id) {
                    nb::gil_scoped_release release;
                    return self.revokeById(key_id);
                },
                nb::arg("key_id"))
            .def("list_admin",
                 [](const mb::MbApiKeyMgr &self) {
                     return mbpy::call_json([&] { return self.listAdmin(); });
                 })
            .def(
                "create_admin",
                [](const mb::MbApiKeyMgr &self, const std::string &user_id,
                   const std::string &label, nb::handle permissions,
                   const std::string &expires_at) {
                    const nlohmann::json perms = permissions_of(permissions);
                    return mbpy::call_json([&] {
                        return self.createAdmin(user_id, label, perms, expires_at);
                    });
                },
                nb::arg("user_id"), nb::arg("label"),
                nb::arg("permissions") = nb::none(), nb::arg("expires_at") = "")
            .def(
                "revoke_admin",
                [](const mb::MbApiKeyMgr &self, const std::string &key_id) {
                    nb::gil_scoped_release release;
                    return self.revokeAdmin(key_id);
                },
                nb::arg("key_id"));

    nb::class_<mb::MbOAuthMgr>(m, "OAuth",
                               "OAuth provider configuration and login")
            .def(
                "authorize_url",
                [](const mb::MbOAuthMgr &self, const std::string &entity,
                   const std::string &provider, const std::string &redirect_uri) {
                    return mbpy::call_json([&] {
                        return self.buildAuthorizeUrl(entity, provider, redirect_uri);
                    });
                },
                nb::arg("entity"), nb::arg("provider"), nb::arg("redirect_uri"))
            .def(
                "handle_callback",
                [](const mb::MbOAuthMgr &self, const std::string &entity,
                   const std::string &provider, const std::string &code,
                   const std::string &state) {
                    return mbpy::call_json([&] {
                        return self.handleCallback(entity, provider, code, state);
                    });
                },
                nb::arg("entity"), nb::arg("provider"), nb::arg("code"),
                nb::arg("state"))
            .def(
                "link_account",
                [](const mb::MbOAuthMgr &self, const std::string &entity,
                   const std::string &user_id, const std::string &provider,
                   const std::string &code, const std::string &state) {
                    return mbpy::call_json([&] {
                        return self.linkAccount(entity, user_id, provider, code, state);
                    });
                },
                nb::arg("entity"), nb::arg("user_id"), nb::arg("provider"),
                nb::arg("code"), nb::arg("state"))
            .def(
                "unlink_account",
                [](const mb::MbOAuthMgr &self, const std::string &entity,
                   const std::string &user_id, const std::string &provider) {
                    nb::gil_scoped_release release;
                    return self.unlinkAccount(entity, user_id, provider);
                },
                nb::arg("entity"), nb::arg("user_id"), nb::arg("provider"))
            .def(
                "linked_accounts",
                [](const mb::MbOAuthMgr &self, const std::string &entity,
                   const std::string &user_id) {
                    return mbpy::call_json(
                        [&] { return self.getLinkedAccounts(entity, user_id); });
                },
                nb::arg("entity"), nb::arg("user_id"))
            .def(
                "providers",
                [](mb::MbOAuthMgr &self, const std::string &entity) {
                    return mbpy::call_json([&] { return self.getProviders(entity); });
                },
                nb::arg("entity"))
            .def(
                "add_provider",
                [](const mb::MbOAuthMgr &self, nb::handle provider) {
                    const nlohmann::json body = mbpy::python_to_json(provider);
                    return mbpy::call_json([&] { return self.addProvider(body); });
                },
                nb::arg("provider"))
            .def(
                "update_provider",
                [](const mb::MbOAuthMgr &self, const std::string &provider_id,
                   nb::handle updates) {
                    const nlohmann::json body = mbpy::python_to_json(updates);
                    return mbpy::call_json(
                        [&] { return self.updateProvider(provider_id, body); });
                },
                nb::arg("provider_id"), nb::arg("updates"))
            .def(
                "remove_provider",
                [](const mb::MbOAuthMgr &self, const std::string &provider_id) {
                    nb::gil_scoped_release release;
                    return self.removeProvider(provider_id);
                },
                nb::arg("provider_id"))
            .def("list_providers",
                 [](const mb::MbOAuthMgr &self) {
                     return mbpy::call_json([&] { return self.listProviders(); });
                 })
            .def(
                "enable_provider",
                [](const mb::MbOAuthMgr &self, const std::string &entity,
                   const std::string &provider_id) {
                    return mbpy::call_json([&] {
                        return self.enableProviderForEntity(entity, provider_id);
                    });
                },
                nb::arg("entity"), nb::arg("provider_id"))
            .def(
                "disable_provider",
                [](const mb::MbOAuthMgr &self, const std::string &entity,
                   const std::string &provider_id) {
                    nb::gil_scoped_release release;
                    return self.disableProviderForEntity(entity, provider_id);
                },
                nb::arg("entity"), nb::arg("provider_id"));

    nb::class_<mb::MbAuth>(m, "MbAuth", "JWT sessions, API keys, and OAuth")
            .def_prop_ro("api_keys", &mb::MbAuth::apiKey,
                         nb::rv_policy::reference_internal)
            .def_prop_ro("oauth", &mb::MbAuth::oauth,
                         nb::rv_policy::reference_internal)
            .def(
                "create_token",
                [](const mb::MbAuth &self, nb::handle claims, int timeout) {
                    const nlohmann::json body = mbpy::python_to_json(claims);
                    std::string token;
                    {
                        nb::gil_scoped_release release;
                        token = self.createToken(body, timeout);
                    }
                    return token;
                },
                nb::arg("claims"), nb::arg("timeout") = -1)
            .def(
                "verify_token",
                [](const mb::MbAuth &self, const std::string &token) {
                    return mbpy::call_json([&] { return self.verifyToken(token); });
                },
                nb::arg("token"))
            .def(
                "delete_session",
                [](const mb::MbAuth &self, const std::string &session_id) {
                    nb::gil_scoped_release release;
                    return self.deleteSession(session_id);
                },
                nb::arg("session_id"))
            .def(
                "refresh_session",
                [](mb::MbAuth &self, const std::string &session_id,
                   const std::string &entity, const std::string &user_id) {
                    return mbpy::call_json([&] {
                        return self.refreshSession(session_id, entity, user_id);
                    });
                },
                nb::arg("session_id"), nb::arg("entity"), nb::arg("user_id"))
            .def(
                "session_timeout",
                [](const mb::MbAuth &self, const std::string &entity, int timeout) {
                    nb::gil_scoped_release release;
                    return self.sessionTimeoutSeconds(entity, timeout);
                },
                nb::arg("entity"), nb::arg("timeout") = -1)
            .def(
                "request_login_otp",
                [](const mb::MbAuth &self, const std::string &entity,
                   const std::string &email) {
                    return mbpy::call_json(
                        [&] { return self.requestLoginOtp(entity, email); });
                },
                nb::arg("entity"), nb::arg("email"))
            .def(
                "login_with_otp",
                [](const mb::MbAuth &self, const std::string &entity,
                   const std::string &email, const std::string &otp) {
                    return mbpy::call_json(
                        [&] { return self.loginWithOtp(entity, email, otp); });
                },
                nb::arg("entity"), nb::arg("email"), nb::arg("otp"))
            .def(
                "request_email_verification",
                [](const mb::MbAuth &self, const std::string &entity,
                   const std::string &email, const std::string &host) {
                    return mbpy::call_json([&] {
                        return self.requestEmailVerification(entity, email, host);
                    });
                },
                nb::arg("entity"), nb::arg("email"), nb::arg("host"))
            .def(
                "confirm_email_verification",
                [](const mb::MbAuth &self, const std::string &entity,
                   const std::string &token) {
                    return mbpy::call_json(
                        [&] { return self.confirmEmailVerification(entity, token); });
                },
                nb::arg("entity"), nb::arg("token"))
            .def(
                "request_password_reset",
                [](const mb::MbAuth &self, const std::string &entity,
                   const std::string &email, const std::string &host) {
                    return mbpy::call_json(
                        [&] { return self.requestPasswordReset(entity, email, host); });
                },
                nb::arg("entity"), nb::arg("email"), nb::arg("host"))
            .def(
                "confirm_password_reset",
                [](const mb::MbAuth &self, const std::string &entity,
                   const std::string &token, const std::string &new_password) {
                    return mbpy::call_json([&] {
                        return self.confirmPasswordReset(entity, token, new_password);
                    });
                },
                nb::arg("entity"), nb::arg("token"), nb::arg("new_password"))
            .def(
                "reject_unverified_login",
                [](const mb::MbAuth &self, const std::string &entity,
                   nb::handle user) {
                    const nlohmann::json body = mbpy::python_to_json(user);
                    nb::gil_scoped_release release;
                    self.rejectUnverifiedLogin(entity, body);
                },
                nb::arg("entity"), nb::arg("user"))
            .def(
                "notify_email_verification",
                [](const mb::MbAuth &self, const std::string &entity,
                   const std::string &user_id) {
                    nb::gil_scoped_release release;
                    self.notifyEmailVerification(entity, user_id);
                },
                nb::arg("entity"), nb::arg("user_id"));
}
