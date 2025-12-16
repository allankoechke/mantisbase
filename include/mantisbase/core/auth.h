/**
 * @file jwt.h
 * @brief Handles JSON Web Token (JWT) creation and verification.
 *
 * Created by allan on 07/06/2025.
 */

#ifndef JWTUNIT_H
#define JWTUNIT_H

#include <string>
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

        /**
         * @brief Verify JWT token and extract claims.
         * @param token JWT token string to verify
         * @return JSON object with:
         *   - "verified": bool indicating if token is valid
         *   - "id": user ID from token
         *   - "entity": entity table name from token
         *   - "error": error message if verification failed
         * @code
         * json result = Auth::verifyToken(token);
         * if (result["verified"]) {
         *     // Token is valid, use result["id"] and result["entity"]
         * }
         * @endcode
         */
        static json verifyToken(const std::string& token);
    };
} // mb

#endif //JWTUNIT_H
