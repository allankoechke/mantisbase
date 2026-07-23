#include "mantisbase/core/oauth.h"
#include "mantisbase/mantisbase.h"
#include "mantisbase/utils/crypto_utils.h"
#include "mantisbase/utils/utils.h"
#include "mantisbase/core/auth.h"

#include <drogon/HttpClient.h>

namespace mb {
    OAuthManager::OAuthManager(MantisBase &app): mApp(app) {}

    std::string OAuthManager::getEncryptionKey() const {
        auto key = getEnvOrDefault("MB_OAUTH_ENCRYPTION_KEY", "");
        if (key.empty()) {
            key = mApp.jwtSecretKey();
        }
        if (key.size() < 32) {
            key.resize(32, '0');
        }
        return key.substr(0, 32);
    }

    json OAuthManager::buildAuthorizeUrl(const std::string &entity_name,
                                         const std::string &provider_name,
                                         const std::string &redirect_uri) const {
        const auto& sql = mApp.db().session();

        soci::row provider_row;
        *sql << "SELECT p.id, p.client_id, p.client_secret_encrypted, p.discovery_url, p.scopes "
                "FROM mb_oauth_providers p "
                "JOIN mb_entity_oauth_config ec ON ec.provider_id = p.id "
                "WHERE p.name = :name AND ec.entity_name = :entity AND ec.enabled = 1 AND p.enabled = 1",
            soci::use(provider_name), soci::use(entity_name), soci::into(provider_row);

        if (!sql->got_data()) {
            throw std::runtime_error("OAuth provider '" + provider_name + "' is not enabled for entity '" + entity_name + "'");
        }

        auto provider_id = provider_row.get<std::string>(0);
        auto client_id = provider_row.get<std::string>(1);
        auto discovery_url = provider_row.get<std::string>(3);
        auto scopes = provider_row.get<std::string>(4);

        auto verifier = generatePKCEVerifier();
        auto challenge = generatePKCEChallenge(verifier);
        auto state = generateSecureRandom(32);
        auto now = getCurrentTimestampUTC();
        auto state_id = generateTimeBasedId();

        // Store state for callback validation (expires in 10 minutes)
        *sql << "INSERT INTO mb_oauth_states (id, state, pkce_verifier, entity_name, provider_id, redirect_uri, expires_at) "
                "VALUES (:id, :state, :verifier, :entity, :provider, :redirect, datetime(:now, '+10 minutes'))",
            soci::use(state_id), soci::use(state), soci::use(verifier),
            soci::use(entity_name), soci::use(provider_id), soci::use(redirect_uri), soci::use(now);

        // Build authorize URL using OIDC discovery if available
        std::string authorize_endpoint;
        if (!discovery_url.empty()) {
            try {
                auto oidc = discoverOIDC(discovery_url);
                authorize_endpoint = oidc.value("authorization_endpoint", "");
            } catch (...) {
                LogOrigin::authWarn("OIDC Discovery Failed", "Failed to discover OIDC for provider " + provider_name);
            }
        }

        // Fallback to well-known endpoints
        if (authorize_endpoint.empty()) {
            if (provider_name == "google") {
                authorize_endpoint = "https://accounts.google.com/o/oauth2/v2/auth";
            } else if (provider_name == "github") {
                authorize_endpoint = "https://github.com/login/oauth/authorize";
            } else if (provider_name == "discord") {
                authorize_endpoint = "https://discord.com/oauth2/authorize";
            } else if (provider_name == "microsoft") {
                authorize_endpoint = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize";
            }
        }

        std::string url = authorize_endpoint +
            "?client_id=" + client_id +
            "&response_type=code" +
            "&redirect_uri=" + redirect_uri +
            "&scope=" + scopes +
            "&state=" + state +
            "&code_challenge=" + challenge +
            "&code_challenge_method=S256";

        return {
            {"authorize_url", url},
            {"state", state}
        };
    }

    json OAuthManager::handleCallback(const std::string &entity_name,
                                       const std::string &provider_name,
                                       const std::string &code,
                                       const std::string &state) const {
        const auto& sql = mApp.db().session();
        auto now = getCurrentTimestampUTC();

        // Validate state
        soci::row state_row;
        *sql << "SELECT id, pkce_verifier, entity_name, provider_id, redirect_uri "
                "FROM mb_oauth_states WHERE state = :state AND expires_at > :now",
            soci::use(state), soci::use(now), soci::into(state_row);

        if (!sql->got_data()) {
            throw std::runtime_error("Invalid or expired OAuth state");
        }

        auto state_id = state_row.get<std::string>(0);
        auto pkce_verifier = state_row.get<std::string>(1);
        auto stored_entity = state_row.get<std::string>(2);
        auto provider_id = state_row.get<std::string>(3);
        auto redirect_uri = state_row.get<std::string>(4);

        if (stored_entity != entity_name) {
            throw std::runtime_error("Entity name mismatch in OAuth state");
        }

        // Delete used state
        *sql << "DELETE FROM mb_oauth_states WHERE id = :id", soci::use(state_id);

        // Get provider details
        soci::row provider_row;
        *sql << "SELECT client_id, client_secret_encrypted, discovery_url, name "
                "FROM mb_oauth_providers WHERE id = :id",
            soci::use(provider_id), soci::into(provider_row);

        if (!sql->got_data()) {
            throw std::runtime_error("OAuth provider not found");
        }

        auto client_id = provider_row.get<std::string>(0);
        auto client_secret_enc = provider_row.get<std::string>(1);
        auto discovery_url = provider_row.get<std::string>(2);

        std::string client_secret;
        if (!client_secret_enc.empty()) {
            try {
                client_secret = aes256GcmDecrypt(client_secret_enc, getEncryptionKey());
            } catch (...) {
                client_secret = client_secret_enc;
            }
        }

        // Get token endpoint
        std::string token_endpoint;
        if (!discovery_url.empty()) {
            try {
                auto oidc = discoverOIDC(discovery_url);
                token_endpoint = oidc.value("token_endpoint", "");
            } catch (...) {}
        }

        if (token_endpoint.empty()) {
            if (provider_name == "google") {
                token_endpoint = "https://oauth2.googleapis.com/token";
            } else if (provider_name == "github") {
                token_endpoint = "https://github.com/login/oauth/access_token";
            } else if (provider_name == "discord") {
                token_endpoint = "https://discord.com/api/oauth2/token";
            } else if (provider_name == "microsoft") {
                token_endpoint = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
            }
        }

        auto tokens = exchangeCode(token_endpoint, code, redirect_uri, client_id, client_secret, pkce_verifier);

        std::string provider_user_id = tokens.value("sub", tokens.value("id", ""));
        std::string email = tokens.value("email", "");

        // Check if user already linked
        soci::row linked_row;
        *sql << "SELECT user_id FROM mb_oauth_accounts "
                "WHERE entity_name = :entity AND provider_id = :pid AND provider_user_id = :puid",
            soci::use(entity_name), soci::use(provider_id), soci::use(provider_user_id),
            soci::into(linked_row);

        std::string user_id;
        bool is_new_user = false;

        if (sql->got_data()) {
            user_id = linked_row.get<std::string>(0);
        } else if (!email.empty()) {
            // Try to find existing user by email
            const auto entity = mApp.entity(entity_name);
            auto opt_user = entity.queryFromCols(email, {"email"});
            if (opt_user.has_value()) {
                user_id = opt_user.value()["id"].get<std::string>();
            } else {
                // Create new user (OAuth-only, no password)
                json new_user;
                new_user["email"] = email;
                new_user["name"] = tokens.value("name", email);
                if (tokens.contains("picture")) {
                    new_user["avatar_url"] = tokens["picture"];
                }
                auto created = entity.create(new_user);
                user_id = created["data"]["id"].get<std::string>();
                is_new_user = true;
            }

            // Link the account
            auto link_id = generateTimeBasedId();
            std::string enc_key = getEncryptionKey();
            std::string access_enc = tokens.contains("access_token")
                ? aes256GcmEncrypt(tokens["access_token"].get<std::string>(), enc_key) : "";
            std::string refresh_enc = tokens.contains("refresh_token")
                ? aes256GcmEncrypt(tokens["refresh_token"].get<std::string>(), enc_key) : "";
            std::string id_token_sub = provider_user_id;

            *sql << "INSERT INTO mb_oauth_accounts (id, entity_name, user_id, provider_id, "
                    "provider_user_id, access_token_encrypted, refresh_token_encrypted, id_token_sub, linked_at) "
                    "VALUES (:id, :entity, :uid, :pid, :puid, :at, :rt, :sub, :now)",
                soci::use(link_id), soci::use(entity_name), soci::use(user_id),
                soci::use(provider_id), soci::use(provider_user_id),
                soci::use(access_enc), soci::use(refresh_enc),
                soci::use(id_token_sub), soci::use(now);
        } else {
            throw std::runtime_error("Cannot identify user from OAuth callback: no email or linked account");
        }

        // Create session token
        auto token = Auth::createToken({{"id", user_id}, {"entity", entity_name}});

        // Get user record
        const auto entity = mApp.entity(entity_name);
        auto user_opt = entity.read(user_id);
        json user = user_opt.has_value() ? user_opt.value() : json::object();
        user.erase("password");

        return {
            {"token", token},
            {"user", user},
            {"is_new_user", is_new_user}
        };
    }

    json OAuthManager::linkAccount(const std::string &entity_name,
                                   const std::string &user_id,
                                   const std::string &provider_name,
                                   const std::string &code,
                                   const std::string &state) const {
        auto result = handleCallback(entity_name, provider_name, code, state);
        return result;
    }

    bool OAuthManager::unlinkAccount(const std::string &entity_name,
                                     const std::string &user_id,
                                     const std::string &provider_name) const {
        const auto& sql = mApp.db().session();
        *sql << "DELETE FROM mb_oauth_accounts "
                "WHERE entity_name = :entity AND user_id = :uid "
                "AND provider_id = (SELECT id FROM mb_oauth_providers WHERE name = :name)",
            soci::use(entity_name), soci::use(user_id), soci::use(provider_name);

        int affected = 0;
        *sql << "SELECT changes()", soci::into(affected);
        return affected > 0;
    }

    json OAuthManager::getLinkedAccounts(const std::string &entity_name,
                                         const std::string &user_id) const {
        const auto& sql = mApp.db().session();
        const soci::rowset<soci::row> rows = (sql->prepare <<
            "SELECT oa.id, p.name, oa.provider_user_id, oa.linked_at "
            "FROM mb_oauth_accounts oa "
            "JOIN mb_oauth_providers p ON p.id = oa.provider_id "
            "WHERE oa.entity_name = :entity AND oa.user_id = :uid",
            soci::use(entity_name), soci::use(user_id));

        json result = json::array();
        for (const auto &row : rows) {
            result.push_back({
                {"id", row.get<std::string>(0)},
                {"provider", row.get<std::string>(1)},
                {"provider_user_id", row.get<std::string>(2)},
                {"linked_at", row.get<std::string>(3)}
            });
        }
        return result;
    }

    json OAuthManager::getProviders(const std::string &entity_name) {
        const auto& sql = mApp.db().session();
        const soci::rowset<soci::row> rows = (sql->prepare <<
            "SELECT p.id, p.name, p.scopes, p.is_preset, ec.enabled "
            "FROM mb_oauth_providers p "
            "LEFT JOIN mb_entity_oauth_config ec ON ec.provider_id = p.id AND ec.entity_name = :entity "
            "WHERE p.enabled = 1",
            soci::use(entity_name));

        json result = json::array();
        for (const auto &row : rows) {
            result.push_back({
                {"id", row.get<std::string>(0)},
                {"name", row.get<std::string>(1)},
                {"scopes", row.get<std::string>(2)},
                {"is_preset", row.get<int>(3) == 1},
                {"enabled_for_entity", row.get_indicator(4) != soci::i_null && row.get<int>(4) == 1}
            });
        }
        return result;
    }

    json OAuthManager::addProvider(const json &provider_data) const {
        const auto& sql = mApp.db().session();
        auto id = generateTimeBasedId();
        auto name = provider_data.at("name").get<std::string>();
        auto client_id = provider_data.at("client_id").get<std::string>();
        const auto client_secret = provider_data.at("client_secret").get<std::string>();
        auto discovery_url = provider_data.value("discovery_url", "");
        auto scopes = provider_data.value("scopes", "openid email profile");

        std::string encrypted_secret = aes256GcmEncrypt(client_secret, getEncryptionKey());

        *sql << "INSERT INTO mb_oauth_providers (id, name, client_id, client_secret_encrypted, "
                "discovery_url, scopes, is_preset, enabled) "
                "VALUES (:id, :name, :cid, :cs, :du, :sc, 0, 1)",
            soci::use(id), soci::use(name), soci::use(client_id),
            soci::use(encrypted_secret), soci::use(discovery_url), soci::use(scopes);

        return {
            {"id", id},
            {"name", name},
            {"client_id", client_id},
            {"discovery_url", discovery_url},
            {"scopes", scopes},
            {"is_preset", false},
            {"enabled", true}
        };
    }

    json OAuthManager::updateProvider(const std::string &provider_id, const json &updates) const {
        const auto& sql = mApp.db().session();

        if (updates.contains("client_id")) {
            auto val = updates["client_id"].get<std::string>();
            *sql << "UPDATE mb_oauth_providers SET client_id = :v WHERE id = :id",
                soci::use(val), soci::use(provider_id);
        }
        if (updates.contains("client_secret")) {
            auto enc = aes256GcmEncrypt(updates["client_secret"].get<std::string>(), getEncryptionKey());
            *sql << "UPDATE mb_oauth_providers SET client_secret_encrypted = :v WHERE id = :id",
                soci::use(enc), soci::use(provider_id);
        }
        if (updates.contains("discovery_url")) {
            auto val = updates["discovery_url"].get<std::string>();
            *sql << "UPDATE mb_oauth_providers SET discovery_url = :v WHERE id = :id",
                soci::use(val), soci::use(provider_id);
        }
        if (updates.contains("scopes")) {
            auto val = updates["scopes"].get<std::string>();
            *sql << "UPDATE mb_oauth_providers SET scopes = :v WHERE id = :id",
                soci::use(val), soci::use(provider_id);
        }
        if (updates.contains("enabled")) {
            int val = updates["enabled"].get<bool>() ? 1 : 0;
            *sql << "UPDATE mb_oauth_providers SET enabled = :v WHERE id = :id",
                soci::use(val), soci::use(provider_id);
        }

        return {{"id", provider_id}, {"updated", true}};
    }

    bool OAuthManager::removeProvider(const std::string &provider_id) const {
        const auto& sql = mApp.db().session();

        // Only allow removing non-preset providers
        int is_preset = 0;
        *sql << "SELECT is_preset FROM mb_oauth_providers WHERE id = :id",
            soci::use(provider_id), soci::into(is_preset);
        if (!sql->got_data()) return false;
        if (is_preset) {
            throw std::runtime_error("Cannot delete preset OAuth provider. Disable it instead.");
        }

        *sql << "DELETE FROM mb_entity_oauth_config WHERE provider_id = :id", soci::use(provider_id);
        *sql << "DELETE FROM mb_oauth_providers WHERE id = :id AND is_preset = 0", soci::use(provider_id);
        int affected = 0;
        *sql << "SELECT changes()", soci::into(affected);
        return affected > 0;
    }

    json OAuthManager::listProviders() const {
        const auto& sql = mApp.db().session();
        const soci::rowset<soci::row> rows = (sql->prepare <<
            "SELECT id, name, client_id, discovery_url, scopes, is_preset, enabled "
            "FROM mb_oauth_providers");

        json result = json::array();
        for (const auto &row : rows) {
            result.push_back({
                {"id", row.get<std::string>(0)},
                {"name", row.get<std::string>(1)},
                {"client_id", row.get<std::string>(2)},
                {"discovery_url", row.get<std::string>(3)},
                {"scopes", row.get<std::string>(4)},
                {"is_preset", row.get<int>(5) == 1},
                {"enabled", row.get<int>(6) == 1}
            });
        }
        return result;
    }

    json OAuthManager::enableProviderForEntity(const std::string &entity_name,
                                               const std::string &provider_id) const {
        const auto& sql = mApp.db().session();
        auto id = generateTimeBasedId();

        // Check if already exists
        int count = 0;
        *sql << "SELECT COUNT(*) FROM mb_entity_oauth_config "
                "WHERE entity_name = :entity AND provider_id = :pid",
            soci::use(entity_name), soci::use(provider_id), soci::into(count);

        if (count > 0) {
            *sql << "UPDATE mb_entity_oauth_config SET enabled = 1 "
                    "WHERE entity_name = :entity AND provider_id = :pid",
                soci::use(entity_name), soci::use(provider_id);
        } else {
            *sql << "INSERT INTO mb_entity_oauth_config (id, entity_name, provider_id, enabled) "
                    "VALUES (:id, :entity, :pid, 1)",
                soci::use(id), soci::use(entity_name), soci::use(provider_id);
        }

        return {{"entity_name", entity_name}, {"provider_id", provider_id}, {"enabled", true}};
    }

    bool OAuthManager::disableProviderForEntity(const std::string &entity_name,
                                                const std::string &provider_id) const {
        const auto& sql = mApp.db().session();
        *sql << "UPDATE mb_entity_oauth_config SET enabled = 0 "
                "WHERE entity_name = :entity AND provider_id = :pid",
            soci::use(entity_name), soci::use(provider_id);
        int affected = 0;
        *sql << "SELECT changes()", soci::into(affected);
        return affected > 0;
    }

    json OAuthManager::discoverOIDC(const std::string &discovery_url) {
        // Use Drogon's HttpClient synchronously
        const auto client = drogon::HttpClient::newHttpClient(discovery_url);
        const auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);

        auto [result, resp] = client->sendRequest(req, 5.0);

        if (result != drogon::ReqResult::Ok || !resp || resp->statusCode() != drogon::k200OK) {
            throw std::runtime_error("OIDC discovery request failed for " + discovery_url);
        }

        return json::parse(std::string(resp->body()));
    }

    json OAuthManager::exchangeCode(const std::string &token_endpoint,
                                    const std::string &code,
                                    const std::string &redirect_uri,
                                    const std::string &client_id,
                                    const std::string &client_secret,
                                    const std::string &pkce_verifier) {
        const auto client = drogon::HttpClient::newHttpClient(token_endpoint);
        const auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setContentTypeString("application/x-www-form-urlencoded");
        req->addHeader("Accept", "application/json");

        const std::string body = "grant_type=authorization_code"
            "&code=" + code +
            "&redirect_uri=" + redirect_uri +
            "&client_id=" + client_id +
            "&client_secret=" + client_secret +
            "&code_verifier=" + pkce_verifier;

        req->setBody(body);

        auto [result, resp] = client->sendRequest(req, 10.0);

        if (result != drogon::ReqResult::Ok || !resp) {
            throw std::runtime_error("Token exchange request failed");
        }

        auto token_data = json::parse(std::string(resp->body()));

        if (token_data.contains("error")) {
            throw std::runtime_error("Token exchange error: " + token_data.value("error_description", token_data.value("error", "unknown")));
        }

        // Decode ID token if present to get user info
        if (token_data.contains("id_token")) {
            auto id_token = token_data["id_token"].get<std::string>();
            // Simple JWT payload extraction (no signature verification for now)
            auto parts = splitString(id_token, ".");
            if (parts.size() >= 2) {
                try {
                    auto payload = base64UrlDecode(parts[1]);
                    auto claims = json::parse(std::string(payload.begin(), payload.end()));
                    if (claims.contains("sub")) token_data["sub"] = claims["sub"];
                    if (claims.contains("email")) token_data["email"] = claims["email"];
                    if (claims.contains("name")) token_data["name"] = claims["name"];
                    if (claims.contains("picture")) token_data["picture"] = claims["picture"];
                } catch (...) {}
            }
        }

        return token_data;
    }
} // mb
