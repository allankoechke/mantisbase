#ifndef MANTISBASE_OAUTH_H
#define MANTISBASE_OAUTH_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

#include "mantisbase/mantisbase.h"

namespace mb {
    using json = nlohmann::json;

    class OAuthManager {
        MantisBase &mApp;

    public:
        explicit OAuthManager(MantisBase &app);

        [[nodiscard]] json buildAuthorizeUrl(const std::string &entity_name,
                                             const std::string &provider_name,
                                             const std::string &redirect_uri) const;

        [[nodiscard]] json handleCallback(const std::string &entity_name,
                                          const std::string &provider_name,
                                          const std::string &code,
                                          const std::string &state) const;

        [[nodiscard]] json linkAccount(const std::string &entity_name,
                                       const std::string &user_id,
                                       const std::string &provider_name,
                                       const std::string &code,
                                       const std::string &state) const;

        [[nodiscard]] bool unlinkAccount(const std::string &entity_name,
                                         const std::string &user_id,
                                         const std::string &provider_name) const;

        [[nodiscard]] json getLinkedAccounts(const std::string &entity_name,
                                             const std::string &user_id) const;

        json getProviders(const std::string &entity_name);

        [[nodiscard]] json addProvider(const json &provider_data) const;

        [[nodiscard]] json updateProvider(const std::string &provider_id, const json &updates) const;

        [[nodiscard]] bool removeProvider(const std::string &provider_id) const;

        [[nodiscard]] json listProviders() const;

        json enableProviderForEntity(const std::string &entity_name,
                                     const std::string &provider_id) const;

        bool disableProviderForEntity(const std::string &entity_name,
                                      const std::string &provider_id) const;

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
