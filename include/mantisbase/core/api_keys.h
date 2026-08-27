/**
 * @file api_keys.h
 * @brief Programmatic and REST-backed API key management.
 *
 * API keys are long-lived credentials prefixed with `mb_sk_`. They authenticate
 * via `Authorization: Bearer mb_sk_...` and are validated by the `getAuthToken()`
 * middleware. Keys are stored hashed; the raw secret is returned only once at creation.
 *
 * Access from C++ through @ref Auth::apiKey() on your owned @ref MantisBase instance.
 *
 * @code
 * auto app = MantisBase::create();
 * auto& keys = app->auth().apiKey();
 *
 * auto created = keys.create("users", user_id, "CI token");
 * // created["key"] is shown once — store it securely
 *
 * auto listed = keys.list("users", user_id); // metadata only, no raw keys
 * keys.revoke(created["id"].get<std::string>(), "users", user_id);
 * @endcode
 */

#ifndef MANTISBASE_API_KEYS_H
#define MANTISBASE_API_KEYS_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

#include "mantisbase/core/types.h"

namespace mb {
    using json = nlohmann::json;

    /** Raw key material returned once from @ref ApiKeyManager::generateApiKey(). */
    struct ApiKeyResult {
        std::string id;
        std::string key;
        std::string key_hash;
    };

    /**
     * @brief Create, list, revoke, and resolve API keys for entity users and admins.
     *
     * REST routes (see @ref auth_api documentation):
     * - `POST|GET /api/v1/auth/<entity>/api-keys`
     * - `DELETE /api/v1/auth/<entity>/api-keys/:id`
     * - `POST|GET /api/v1/sys/api-keys` (admin)
     * - `DELETE /api/v1/sys/api-keys/:id` (admin)
     */
    class ApiKeyManager: public IMantisBase {
    public:
        explicit ApiKeyManager(const MantisBase& app);

        /** Generate a new key id, raw secret (`mb_sk_...`), and SHA-256 hash. */
        static ApiKeyResult generateApiKey();
        static std::string hashApiKey(const std::string &raw_key);

        /** Create an entity-scoped key for `user_id`. Returns JSON including one-time `key`. */
        [[nodiscard]] json create(const std::string &entity_name, const std::string &user_id,
                          const std::string &label, const json &permissions = json::array(),
                          const std::string &expires_at = "") const;

        /** List key metadata for a user/entity (never includes raw secrets). */
        [[nodiscard]] json list(const std::string &entity_name, const std::string &user_id) const;

        /** Revoke a key owned by `user_id` within an entity. Pass empty `user_id` to skip owner check. */
        [[nodiscard]] bool revoke(const std::string &key_id, const std::string &entity_name,
                          const std::string &user_id) const;

        /** Revoke any key by id (admin use). */
        [[nodiscard]] bool revokeById(const std::string &key_id) const;

        /** Resolve a stored key by hash; used by auth middleware. */
        [[nodiscard]] std::optional<json> lookupByHash(const std::string &key_hash) const;

        /** Admin: list all system API keys (metadata only). */
        [[nodiscard]] json listAdmin() const;

        /** Admin: create a system API key for an admin user. Returns one-time `key`. */
        [[nodiscard]] json createAdmin(const std::string &user_id, const std::string &label,
                               const json &permissions = json::array(),
                               const std::string &expires_at = "") const;

        /** Revoke any admin API key by id (any admin may revoke any admin key). */
        [[nodiscard]] bool revokeAdmin(const std::string &key_id) const;
    };
} // mb

#endif // MANTISBASE_API_KEYS_H
