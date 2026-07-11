#ifndef MANTISBASE_OAUTH_H
#define MANTISBASE_OAUTH_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace mb {
    using json = nlohmann::json;

    class OAuthManager {
    public:
        static json buildAuthorizeUrl(const std::string &entity_name,
                                      const std::string &provider_name,
                                      const std::string &redirect_uri);

        static json handleCallback(const std::string &entity_name,
                                   const std::string &provider_name,
                                   const std::string &code,
                                   const std::string &state);

        static json linkAccount(const std::string &entity_name,
                               const std::string &user_id,
                               const std::string &provider_name,
                               const std::string &code,
                               const std::string &state);

        static bool unlinkAccount(const std::string &entity_name,
                                 const std::string &user_id,
                                 const std::string &provider_name);

        static json getLinkedAccounts(const std::string &entity_name,
                                     const std::string &user_id);

        static json getProviders(const std::string &entity_name);

        static json addProvider(const json &provider_data);
        static json updateProvider(const std::string &provider_id, const json &updates);
        static bool removeProvider(const std::string &provider_id);
        static json listProviders();

        static json enableProviderForEntity(const std::string &entity_name,
                                           const std::string &provider_id);
        static bool disableProviderForEntity(const std::string &entity_name,
                                            const std::string &provider_id);

        static std::string getEncryptionKey();

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
