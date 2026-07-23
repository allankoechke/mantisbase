#ifndef MANTISBASE_API_KEYS_H
#define MANTISBASE_API_KEYS_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace mb {
    using json = nlohmann::json;

    struct ApiKeyResult {
        std::string id;
        std::string key;
        std::string key_hash;
    };

    class ApiKeyManager {
    public:
        static ApiKeyResult generateApiKey();
        static std::string hashApiKey(const std::string &raw_key);

        static json create(const std::string &entity_name, const std::string &user_id,
                          const std::string &label, const json &permissions = json::array(),
                          const std::string &expires_at = "");

        static json list(const std::string &entity_name, const std::string &user_id);

        static bool revoke(const std::string &key_id, const std::string &entity_name,
                          const std::string &user_id);

        static std::optional<json> lookupByHash(const std::string &key_hash);

        static json listAdmin();

        static json createAdmin(const std::string &user_id, const std::string &label,
                               const json &permissions = json::array(),
                               const std::string &expires_at = "");

        static bool revokeAdmin(const std::string &key_id, const std::string &user_id);
    };
} // mb

#endif // MANTISBASE_API_KEYS_H
