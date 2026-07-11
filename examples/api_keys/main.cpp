/**
 * API key management example for MantisBase.
 *
 * Demonstrates issuing an API key, using it as a Bearer token,
 * and revoking it. API keys provide an alternative to JWT-based
 * authentication for programmatic access.
 *
 * Usage:
 *   ./api_keys_example
 *
 * API key endpoints (after entity "users" is created):
 *   POST   /api/v1/auth/users/api-keys      - Issue a new API key
 *   GET    /api/v1/auth/users/api-keys      - List user's API keys
 *   DELETE /api/v1/auth/users/api-keys/:id  - Revoke an API key
 *
 * Admin API key endpoints:
 *   POST   /api/v1/sys/api-keys      - Issue admin API key
 *   GET    /api/v1/sys/api-keys      - List admin API keys
 *   DELETE /api/v1/sys/api-keys/:id  - Revoke admin API key
 *
 * Example workflow:
 *   1. Login to get a JWT: POST /api/v1/auth/users/login
 *   2. Issue an API key:
 *      POST /api/v1/auth/users/api-keys
 *      Authorization: Bearer <jwt>
 *      {"label": "CI Pipeline", "permissions": ["read"]}
 *
 *      Response: {"id": "...", "key": "mb_sk_abc123..."}
 *      (The raw key is shown ONCE — store it securely)
 *
 *   3. Use the API key for requests:
 *      GET /api/v1/users/records
 *      Authorization: Bearer mb_sk_abc123...
 *
 *   4. Revoke when no longer needed:
 *      DELETE /api/v1/auth/users/api-keys/<id>
 *      Authorization: Bearer <jwt>
 */

#include <mantisbase/mantis.h>
#include <iostream>

int main()
{
    mb::json config;
    config["dev"] = true;
    config["database"] = "SQLITE";
    config["dataDir"] = "./data";
    config["publicDir"] = "./www";
    config["serve"] = {{"port", 7070}, {"host", "0.0.0.0"}, {"skip-admin-setup", true}};

    auto& app = mb::MantisBase::create(config);

    std::cout << "MantisBase API Keys Example" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << std::endl;
    std::cout << "Steps to use API keys:" << std::endl;
    std::cout << "  1. Set up admin: POST /api/v1/sys/admins/setup" << std::endl;
    std::cout << "     {\"email\": \"admin@example.com\", \"password\": \"secret\"}" << std::endl;
    std::cout << std::endl;
    std::cout << "  2. Login as admin: POST /api/v1/sys/admins/login" << std::endl;
    std::cout << "     {\"email\": \"admin@example.com\", \"password\": \"secret\"}" << std::endl;
    std::cout << std::endl;
    std::cout << "  3. Issue admin API key:" << std::endl;
    std::cout << "     POST /api/v1/sys/api-keys" << std::endl;
    std::cout << "     Authorization: Bearer <jwt-from-login>" << std::endl;
    std::cout << "     {\"label\": \"My Script\"}" << std::endl;
    std::cout << std::endl;
    std::cout << "  4. Response contains: {\"key\": \"mb_sk_...\"}" << std::endl;
    std::cout << "     Save this key! It is shown only once." << std::endl;
    std::cout << std::endl;
    std::cout << "  5. Use the key for authenticated requests:" << std::endl;
    std::cout << "     curl -H 'Authorization: Bearer mb_sk_...' \\" << std::endl;
    std::cout << "       http://localhost:" << app.port() << "/api/v1/health" << std::endl;
    std::cout << std::endl;
    std::cout << "Server starting on http://localhost:" << app.port() << std::endl;

    return app.run();
}
