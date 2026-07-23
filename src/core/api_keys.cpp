#include "mantisbase/core/api_keys.h"
#include "mantisbase/mantisbase.h"
#include "mantisbase/utils/crypto_utils.h"
#include "mantisbase/utils/utils.h"

namespace mb {
    ApiKeyResult ApiKeyManager::generateApiKey() {
        const auto random_hex = generateSecureRandom(32);
        const std::string raw_key = "mb_sk_" + random_hex;
        const std::string key_hash = hashApiKey(raw_key);
        const std::string id = generateTimeBasedId();

        return {.id = id, .key = raw_key, .key_hash = key_hash};
    }

    std::string ApiKeyManager::hashApiKey(const std::string &raw_key) {
        return sha256Hex(raw_key);
    }

    json ApiKeyManager::create(const std::string &entity_name, const std::string &user_id,
                               const std::string &label, const json &permissions,
                               const std::string &expires_at) const {
        auto [id, key, key_hash] = generateApiKey();
        auto now = getCurrentTimestampUTC();

        const auto& sql = mApp.db().session();

        std::string perms_str = permissions.dump();
        std::string exp = expires_at.empty() ? "" : expires_at;

        if (exp.empty()) {
            *sql << "INSERT INTO mb_api_keys (id, entity_name, user_id, key_hash, label, permissions, created) "
                    "VALUES (:id, :entity, :user_id, :hash, :label, :perms, :created)",
                soci::use(id), soci::use(entity_name), soci::use(user_id),
                soci::use(key_hash), soci::use(label), soci::use(perms_str), soci::use(now);
        } else {
            *sql << "INSERT INTO mb_api_keys (id, entity_name, user_id, key_hash, label, permissions, created, expires_at) "
                    "VALUES (:id, :entity, :user_id, :hash, :label, :perms, :created, :exp)",
                soci::use(id), soci::use(entity_name), soci::use(user_id),
                soci::use(key_hash), soci::use(label), soci::use(perms_str), soci::use(now), soci::use(exp);
        }

        return {
            {"id", id},
            {"key", key},
            {"label", label},
            {"entity_name", entity_name},
            {"permissions", permissions},
            {"created", now},
            {"expires_at", expires_at.empty() ? json(nullptr) : json(expires_at)}
        };
    }

    json ApiKeyManager::list(const std::string &entity_name, const std::string &user_id) const {
        const auto& sql = mApp.db().session();
        const soci::rowset<soci::row> rows = (sql->prepare <<
            "SELECT id, entity_name, user_id, label, permissions, last_used, created, expires_at "
            "FROM mb_api_keys WHERE entity_name = :entity AND user_id = :uid",
            soci::use(entity_name), soci::use(user_id));

        json result = json::array();
        for (const auto &row : rows) {
            json item;
            item["id"] = row.get<std::string>(0);
            item["entity_name"] = row.get<std::string>(1);
            item["user_id"] = row.get<std::string>(2);
            item["label"] = row.get<std::string>(3);
            item["permissions"] = json::parse(row.get<std::string>(4, "[]"));
            item["last_used"] = row.get_indicator(5) == soci::i_null ? json(nullptr) : json(row.get<std::string>(5));
            item["created"] = row.get<std::string>(6);
            item["expires_at"] = row.get_indicator(7) == soci::i_null ? json(nullptr) : json(row.get<std::string>(7));
            result.push_back(item);
        }
        return result;
    }

    bool ApiKeyManager::revoke(const std::string &key_id, const std::string &entity_name,
                               const std::string &user_id) const {
        const auto& sql = mApp.db().session();
        *sql << "DELETE FROM mb_api_keys WHERE id = :id AND entity_name = :entity AND user_id = :uid",
            soci::use(key_id), soci::use(entity_name), soci::use(user_id);

        int affected = 0;
        *sql << "SELECT changes()", soci::into(affected);
        return affected > 0;
    }

    std::optional<json> ApiKeyManager::lookupByHash(const std::string &key_hash) const {
        const auto& sql = mApp.db().session();
        soci::row row;
        *sql << "SELECT id, entity_name, user_id, label, permissions, last_used, created, expires_at "
                "FROM mb_api_keys WHERE key_hash = :hash",
            soci::use(key_hash), soci::into(row);

        if (!sql->got_data()) {
            return std::nullopt;
        }

        auto now = getCurrentTimestampUTC();
        auto key_id = row.get<std::string>(0);
        *sql << "UPDATE mb_api_keys SET last_used = :now WHERE id = :id",
            soci::use(now), soci::use(key_id);

        // Check expiration
        if (row.get_indicator(7) != soci::i_null) {
            auto exp = row.get<std::string>(7);
            if (!exp.empty() && exp < now) {
                return std::nullopt;
            }
        }

        json result;
        result["id"] = row.get<std::string>(0);
        result["entity_name"] = row.get<std::string>(1);
        result["user_id"] = row.get<std::string>(2);
        result["label"] = row.get<std::string>(3);
        result["permissions"] = json::parse(row.get<std::string>(4, "[]"));
        result["last_used"] = now;
        result["created"] = row.get<std::string>(6);
        result["expires_at"] = row.get_indicator(7) == soci::i_null ? json(nullptr) : json(row.get<std::string>(7));
        return result;
    }

    json ApiKeyManager::listAdmin() {
        return list("mb_admins", "");
    }

    json ApiKeyManager::createAdmin(const std::string &user_id, const std::string &label,
                                    const json &permissions, const std::string &expires_at) const {
        return create("mb_admins", user_id, label, permissions, expires_at);
    }

    bool ApiKeyManager::revokeAdmin(const std::string &key_id, const std::string &user_id) const {
        return revoke(key_id, "mb_admins", user_id);
    }
} // mb
