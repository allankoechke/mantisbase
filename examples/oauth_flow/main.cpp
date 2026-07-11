/**
 * OAuth flow example for MantisBase.
 *
 * Demonstrates configuring Google OAuth for an auth entity.
 * After starting, use the admin API to:
 *   1. Configure a Google OAuth provider with your client credentials
 *   2. Enable it for an auth entity
 *   3. Users can then authenticate via the OAuth flow
 *
 * Usage:
 *   ./oauth_flow
 *
 * Environment variables:
 *   GOOGLE_CLIENT_ID     - Your Google OAuth client ID
 *   GOOGLE_CLIENT_SECRET - Your Google OAuth client secret
 *
 * OAuth endpoints (after entity "users" is created):
 *   GET  /api/v1/auth/users/oauth/providers      - List enabled providers
 *   GET  /api/v1/auth/users/oauth/authorize/google - Start OAuth flow
 *   GET  /api/v1/auth/users/oauth/callback/google  - OAuth callback
 *
 * Admin endpoints:
 *   POST /api/v1/sys/oauth/providers      - Add/configure a provider
 *   GET  /api/v1/sys/oauth/providers      - List all providers
 *   POST /api/v1/sys/oauth/entity-config  - Enable provider for entity
 */

#include <mantisbase/mantis.h>
#include <iostream>
#include <cstdlib>

int main()
{
    mb::json config;
    config["dev"] = true;
    config["database"] = "SQLITE";
    config["dataDir"] = "./data";
    config["publicDir"] = "./www";
    config["serve"] = {{"port", 7070}, {"host", "0.0.0.0"}, {"skip-admin-setup", true}};

    auto& app = mb::MantisBase::create(config);

    const char* clientId = std::getenv("GOOGLE_CLIENT_ID");
    const char* clientSecret = std::getenv("GOOGLE_CLIENT_SECRET");

    std::cout << "MantisBase OAuth Example" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << std::endl;

    if (!clientId || !clientSecret) {
        std::cout << "Note: GOOGLE_CLIENT_ID and GOOGLE_CLIENT_SECRET not set." << std::endl;
        std::cout << "The server will start, but OAuth won't work until you" << std::endl;
        std::cout << "configure a provider via the admin API." << std::endl;
    } else {
        std::cout << "Google OAuth credentials detected." << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Steps to set up OAuth:" << std::endl;
    std::cout << "  1. Set up admin: POST /api/v1/sys/admins/setup" << std::endl;
    std::cout << "  2. Create auth entity 'users' with POST /api/v1/schemas" << std::endl;
    std::cout << "  3. Configure Google provider:" << std::endl;
    std::cout << "     POST /api/v1/sys/oauth/providers" << std::endl;
    std::cout << "     {" << std::endl;
    std::cout << "       \"name\": \"google\"," << std::endl;
    std::cout << "       \"client_id\": \"<your-client-id>\"," << std::endl;
    std::cout << "       \"client_secret\": \"<your-client-secret>\"," << std::endl;
    std::cout << "       \"discovery_url\": \"https://accounts.google.com/.well-known/openid-configuration\"," << std::endl;
    std::cout << "       \"scopes\": \"openid email profile\"" << std::endl;
    std::cout << "     }" << std::endl;
    std::cout << "  4. Enable for entity:" << std::endl;
    std::cout << "     POST /api/v1/sys/oauth/entity-config" << std::endl;
    std::cout << "     {\"entity_name\": \"users\", \"provider_id\": \"<id>\"}" << std::endl;
    std::cout << "  5. Users visit: GET /api/v1/auth/users/oauth/authorize/google" << std::endl;
    std::cout << std::endl;
    std::cout << "Server starting on http://localhost:" << app.port() << std::endl;

    return app.run();
}
