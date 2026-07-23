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
#include "../utils/utils.h"

namespace mb
{
    /**
     * @brief JWT token creation and verification utilities.
     *
     * Provides static methods for generating and validating JWT tokens
     * used for authentication and authorization.
     *
     * @code
     * // Create token for user
     * json claims = {{"id", "user123"}, {"table", "users"}};
     * std::string token = Auth::createToken(claims, 3600); // 1 hour
     *
     * // Verify token
     * json result = Auth::verifyToken(token);
     * if (result["verified"].get<bool>()) {
     *     std::string userId = result["id"];
     * }
     * @endcode
     */
    class Auth
    {
    public:
        /**
         * @brief Create JWT token with custom claims.
         * @param claims_params JSON object with claims (must include "id" and "table")
         * @param timeout Token expiration in seconds (-1 for default, typically 1 hour)
         * @return JWT token string
         * @code
         * json claims = {{"id", "user123"}, {"table", "users"}};
         * std::string token = Auth::createToken(claims, 3600);
         * @endcode
         */
        static std::string createToken(const json& claims_params, int timeout = -1);

        static json verifyToken(const std::string& token);

        static bool deleteSession(const std::string& session_id);

        static json refreshSession(const std::string& old_session_id, const std::string& entity_name,
                                   const std::string& user_id);
    };
} // mb

#endif //JWTUNIT_H
