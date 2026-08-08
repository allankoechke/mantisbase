/**
 * @file jwt.h
 * @brief Handles JSON Web Token (JWT) creation and verification.
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
        /// Create Auth instance given a `const MantisBase&`
        explicit Auth(const MantisBase& app);

        /// Return a ref to OAuthManger instance
        [[nodiscard]] OAuthManager& oauth() const;

        /// Return a ref to ApiKeyManager instance
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

        json verifyToken(const std::string& token) const;

        bool deleteSession(const std::string& session_id) const;

        json refreshSession(const std::string& old_session_id, const std::string& entity_name,
                                   const std::string& user_id);
    };
} // mb

#endif //JWTUNIT_H
