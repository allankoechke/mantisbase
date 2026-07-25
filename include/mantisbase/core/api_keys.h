#ifndef MANTISBASE_API_KEYS_H
#define MANTISBASE_API_KEYS_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

#include "mantisbase/core/types.h"

namespace mb {
    using json = nlohmann::json;

    struct ApiKeyResult {
        std::string id;
        std::string key;
        std::string key_hash;
    };

    class ApiKeyManager: public IMantisBase {
    public:
        explicit ApiKeyManager(const MantisBase& app);

        static ApiKeyResult generateApiKey();
        static std::string hashApiKey(const std::string &raw_key);

        [[nodiscard]] json create(const std::string &entity_name, const std::string &user_id,
                          const std::string &label, const json &permissions = json::array(),
                          const std::string &expires_at = "") const;

        [[nodiscard]] json list(const std::string &entity_name, const std::string &user_id) const;

        [[nodiscard]] bool revoke(const std::string &key_id, const std::string &entity_name,
                          const std::string &user_id) const;

        [[nodiscard]] std::optional<json> lookupByHash(const std::string &key_hash) const;

        [[nodiscard]] json listAdmin() const;

        [[nodiscard]] json createAdmin(const std::string &user_id, const std::string &label,
                               const json &permissions = json::array(),
                               const std::string &expires_at = "") const;

        [[nodiscard]] bool revokeAdmin(const std::string &key_id, const std::string &user_id) const;
    };
} // mb

#endif // MANTISBASE_API_KEYS_H
