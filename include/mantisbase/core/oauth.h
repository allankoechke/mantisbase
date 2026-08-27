/**
 * @file oauth.h
 * @brief OAuth 2.0 / OIDC provider configuration and user login flows.
 *
 * OAuth is configured per auth-type entity. Preset providers (Google, GitHub,
 * Discord, Microsoft) are seeded at startup; admins supply `client_id` and
 * `client_secret` before enabling them for an entity.
 *
 * Access from C++ through @ref Auth::oauth() on your owned @ref MantisBase instance.
 *
 * @code
 * auto app = MantisBase::create();
 * auto& oauth = app->auth().oauth();
 *
 * // Admin: register or update a provider, then enable it for an entity
 * oauth.addProvider({{"name", "google"}, {"client_id", "..."}, {"client_secret", "..."}});
 * oauth.enableProviderForEntity("users", provider_id);
 *
 * // User-facing authorize URL (PKCE)
 * auto url = oauth.buildAuthorizeUrl("users", "google", redirect_uri);
 * @endcode
 */

#ifndef MANTISBASE_OAUTH_H
#define MANTISBASE_OAUTH_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

#include "mantisbase/core/types.h"

namespace mb {
    using json = nlohmann::json;

    /**
     * @brief OAuth provider registry, entity enablement, and login/link flows.
     *
     * REST routes (see @ref auth_api documentation):
     * - `GET /api/v1/auth/<entity>/oauth/authorize/:provider`
     * - `GET /api/v1/auth/<entity>/oauth/callback/:provider`
     * - `POST|DELETE /api/v1/auth/<entity>/oauth/link/:provider`
     * - `GET /api/v1/auth/<entity>/oauth/accounts`
     * - `GET /api/v1/auth/<entity>/oauth/providers`
     * - `POST|GET|PATCH|DELETE /api/v1/sys/oauth/providers` (admin)
     * - `POST|DELETE /api/v1/sys/oauth/entity-config` (admin)
     */
    class OAuthManager : public IMantisBase {
    public:
        explicit OAuthManager(const MantisBase &app);

        /** Build provider authorize URL and persist PKCE state server-side. */
        [[nodiscard]] json buildAuthorizeUrl(const std::string &entity_name,
                                             const std::string &provider_name,
                                             const std::string &redirect_uri) const;

        /** Exchange callback `code`/`state` for a session JWT (login or signup). */
        [[nodiscard]] json handleCallback(const std::string &entity_name,
                                          const std::string &provider_name,
                                          const std::string &code,
                                          const std::string &state) const;

        /** Link an OAuth identity to an already authenticated user. */
        [[nodiscard]] json linkAccount(const std::string &entity_name,
                                       const std::string &user_id,
                                       const std::string &provider_name,
                                       const std::string &code,
                                       const std::string &state) const;

        /** Unlink an OAuth provider from a user account. */
        [[nodiscard]] bool unlinkAccount(const std::string &entity_name,
                                         const std::string &user_id,
                                         const std::string &provider_name) const;

        /** List OAuth identities linked to @p user_id within @p entity_name. */
        [[nodiscard]] json getLinkedAccounts(const std::string &entity_name,
                                             const std::string &user_id) const;

        /** List OAuth providers enabled for an entity (user-facing metadata). */
        json getProviders(const std::string &entity_name);

        /** Admin: create a provider configuration row. */
        [[nodiscard]] json addProvider(const json &provider_data) const;

        /** Admin: patch provider settings (client id/secret, endpoints, etc.). */
        [[nodiscard]] json updateProvider(const std::string &provider_id, const json &updates) const;

        /** Admin: delete a provider by id. */
        [[nodiscard]] bool removeProvider(const std::string &provider_id) const;

        /** Admin: list all configured providers. */
        [[nodiscard]] json listProviders() const;

        /** Admin: attach a provider to an entity's allowed login methods. */
        json enableProviderForEntity(const std::string &entity_name,
                                     const std::string &provider_id) const;

        /** Admin: detach a provider from an entity. */
        bool disableProviderForEntity(const std::string &entity_name,
                                      const std::string &provider_id) const;

        /** @return AES key material used to encrypt stored OAuth client secrets. */
        std::string getEncryptionKey() const;

    private:
        static json discoverOIDC(const std::string &discovery_url);

        static json exchangeCode(const std::string &token_endpoint,
                                 const std::string &code,
                                 const std::string &redirect_uri,
                                 const std::string &client_id,
                                 const std::string &client_secret,
                                 const std::string &pkce_verifier);
    };
} // mb

#endif // MANTISBASE_OAUTH_H
