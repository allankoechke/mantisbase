/**
 * @file auth.h
 * @brief JWT session management and entry point for API keys and OAuth.
 *
 * Created by allan on 07/06/2025.
 */

#ifndef JWTUNIT_H
#define JWTUNIT_H

#include <string>
#include <wolfssl/wolfio.h>

#include "api_keys.h"
#include "oauth.h"
#include "../core/types.h"

namespace mb
{
    /// HttpOnly cookie name used for JWT session tokens on login/refresh responses.
    inline constexpr const char kAuthTokenCookieName[] = "mb_token";

    /**
     * @brief JWT token creation and verification utilities.
     *
     * Provides static methods for generating and validating JWT tokens
     * used for authentication and authorization.
     *
     * @code
     * auto app = MantisBase::create();
     * auto& auth = app->auth();
     *
     * json claims = {{"id", "user123"}, {"table", "users"}};
     * std::string token = auth.createToken(claims, 3600); // 1 hour
     *
     * json result = auth.verifyToken(token);
     * if (result["verified"].get<bool>()) {
     *     std::string userId = result["id"];
     * }
     *
     * // API keys and OAuth live on the same Auth instance:
     * auth.apiKey().create("users", "user123", "Mobile app");
     * auth.oauth().buildAuthorizeUrl("users", "google", redirect_uri);
     * @endcode
     */
    class Auth
    {
        const MantisBase& mApp;
        std::unique_ptr<OAuthManager> m_oauth;
        std::unique_ptr<ApiKeyManager> m_apiKeyManager;

    public:
        /** Construct auth services bound to @p app (OAuth + API keys). */
        explicit Auth(const MantisBase& app);

        /** @return OAuth manager for provider configuration and login flows. */
        [[nodiscard]] OAuthManager& oauth() const;

        /** @return API key manager for programmatic credentials. */
        [[nodiscard]] ApiKeyManager& apiKey() const;

        /**
         * @brief Create JWT token with custom claims.
         * @param claims_params JSON object with claims (must include "id" and "table")
         * @param timeout Token expiration in seconds (-1 for default, typically 1 hour)
         * @return JWT token string
         * @code
         * std::string token = app.auth().createToken(
         *     {{"id", "user123"}, {"table", "users"}}, 3600);
         * @endcode
         */
        std::string createToken(const json& claims_params, int timeout = -1) const;

        /** Default session lifetime in seconds for an entity (or @p timeout when positive). */
        [[nodiscard]] int sessionTimeoutSeconds(const std::string &entity_name, int timeout = -1) const;

        /**
         * @brief Verify a JWT and return claims plus verification metadata.
         * @return JSON with `verified`, `error`, and claim fields on success/failure.
         */
        json verifyToken(const std::string& token) const;

        /** Invalidate a refresh/session row by id. */
        bool deleteSession(const std::string& session_id) const;

        /**
         * @brief Rotate a session: invalidate `old_session_id` and issue a new JWT.
         * @return JSON with new `token` on success.
         */
        json refreshSession(const std::string& old_session_id, const std::string& entity_name,
                                   const std::string& user_id);

#ifdef MB_SCRIPTING_ENABLED
        std::string createTokenJson(const std::string &claims_json, int timeout = -1) const;
        std::string verifyTokenJson(const std::string &token) const;
        std::string refreshSessionJson(const std::string &old_session_id,
                                       const std::string &entity_name,
                                       const std::string &user_id);
#endif
    };
} // mb

#endif //JWTUNIT_H
