#include <gtest/gtest.h>
#include "mantisbase/core/api_keys.h"
#include "mantisbase/utils/crypto_utils.h"
#include "mantisbase/mantisbase.h"
#include "../common/test_fixture.h"
#include "../common/test_config.h"

class ApiKeyTestFixture : public MbAppFixture {
};

TEST_F(ApiKeyTestFixture, GenerateApiKey) {
    auto [id, key, key_hash] = mb::ApiKeyManager::generateApiKey();

    EXPECT_FALSE(id.empty());
    EXPECT_TRUE(key.starts_with("mb_sk_"));
    EXPECT_FALSE(key_hash.empty());
    EXPECT_EQ(key_hash.size(), 64u);

}

TEST_F(ApiKeyTestFixture, GenerateUniqueKeys) {
    auto [id1, key1, key_hash1] = mb::ApiKeyManager::generateApiKey();
    auto [id2, key2, key_hash2] = mb::ApiKeyManager::generateApiKey();

    EXPECT_NE(key1, key2);
    EXPECT_NE(key_hash1, key_hash2);
    EXPECT_NE(id1, id2);
}

TEST_F(ApiKeyTestFixture, HashApiKeyConsistency) {
    const std::string raw_key = "mb_sk_abcdef1234567890abcdef1234567890abcdef1234567890abcdef12345678";
    const auto hash1 = mb::ApiKeyManager::hashApiKey(raw_key);
    const auto hash2 = mb::ApiKeyManager::hashApiKey(raw_key);

    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.size(), 64u);
}

TEST_F(ApiKeyTestFixture, DifferentKeysDifferentHashes) {
    const std::string key1 = "mb_sk_aaaa";
    const std::string key2 = "mb_sk_bbbb";

    const auto hash1 = mb::ApiKeyManager::hashApiKey(key1);
    const auto hash2 = mb::ApiKeyManager::hashApiKey(key2);

    EXPECT_NE(hash1, hash2);
}

TEST_F(ApiKeyTestFixture, CreateAndLookupApiKey) {
    const auto &keys = mantis().auth().apiKey();
    auto result = keys.create("mb_admins", "test_user_id", "Test Key");

    EXPECT_TRUE(result.contains("id"));
    EXPECT_TRUE(result.contains("key"));
    EXPECT_TRUE(result["key"].get<std::string>().starts_with("mb_sk_"));
    EXPECT_EQ(result["label"].get<std::string>(), "Test Key");

    const auto key_hash = mb::ApiKeyManager::hashApiKey(result["key"].get<std::string>());
    auto lookup = keys.lookupByHash(key_hash);

    EXPECT_TRUE(lookup.has_value());
    EXPECT_EQ(lookup.value()["user_id"].get<std::string>(), "test_user_id");
    EXPECT_EQ(lookup.value()["entity_name"].get<std::string>(), "mb_admins");
}

TEST_F(ApiKeyTestFixture, LookupNonExistentKey) {
    const auto lookup = mantis().auth().apiKey().lookupByHash("nonexistent_hash_value_that_does_not_exist");
    EXPECT_FALSE(lookup.has_value());
}

TEST_F(ApiKeyTestFixture, CreateListAndRevokeApiKey) {
    const auto &keys = mantis().auth().apiKey();
    auto created = keys.create("mb_admins", "list_test_user", "List Test Key");
    const auto key_id = created["id"].get<std::string>();

    auto key_list = keys.list("mb_admins", "list_test_user");
    EXPECT_GE(key_list.size(), 1u);

    bool found = false;
    for (const auto &k : key_list) {
        if (k["id"].get<std::string>() == key_id) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    const bool revoked = keys.revoke(key_id, "mb_admins", "list_test_user");
    EXPECT_TRUE(revoked);

    const auto key_hash = mb::ApiKeyManager::hashApiKey(created["key"].get<std::string>());
    const auto lookup = keys.lookupByHash(key_hash);
    EXPECT_FALSE(lookup.has_value());
}

TEST_F(ApiKeyTestFixture, RevokeNonExistentKey) {
    const bool revoked = mantis().auth().apiKey().revoke("nonexistent_id", "mb_admins", "no_user");
    EXPECT_FALSE(revoked);
}

TEST_F(ApiKeyTestFixture, RevokeByIdAllowsAdminToRevokeAnyKey) {
    const auto &keys = mantis().auth().apiKey();
    auto created = keys.create("mb_admins", "owner_user", "Owner Key");
    const auto key_id = created["id"].get<std::string>();

    EXPECT_FALSE(keys.revoke(key_id, "mb_admins", "other_user"));
    EXPECT_TRUE(keys.revokeById(key_id));

    const auto key_hash = mb::ApiKeyManager::hashApiKey(created["key"].get<std::string>());
    EXPECT_FALSE(keys.lookupByHash(key_hash).has_value());
}

TEST_F(ApiKeyTestFixture, RevokeWithoutUserIdSkipsOwnerCheck) {
    const auto &keys = mantis().auth().apiKey();
    auto created = keys.create("mb_admins", "owner_user", "Scoped Key");
    const auto key_id = created["id"].get<std::string>();

    EXPECT_TRUE(keys.revoke(key_id, "mb_admins", ""));

    const auto key_hash = mb::ApiKeyManager::hashApiKey(created["key"].get<std::string>());
    EXPECT_FALSE(keys.lookupByHash(key_hash).has_value());
}

TEST_F(ApiKeyTestFixture, RevokeAdminUsesGlobalId) {
    const auto &keys = mantis().auth().apiKey();
    auto created = keys.createAdmin("some_admin", "Admin Key");
    const auto key_id = created["id"].get<std::string>();

    EXPECT_TRUE(keys.revokeAdmin(key_id));
    EXPECT_FALSE(keys.revokeAdmin("missing_id"));
}

TEST_F(ApiKeyTestFixture, ShownOnceVerification) {
    const auto &keys = mantis().auth().apiKey();
    auto result = keys.create("mb_admins", "shown_once_user", "Shown Once Key");

    EXPECT_TRUE(result.contains("key"));
    const auto raw_key = result["key"].get<std::string>();
    EXPECT_TRUE(raw_key.starts_with("mb_sk_"));

    auto key_list = keys.list("mb_admins", "shown_once_user");
    for (const auto &k : key_list) {
        EXPECT_FALSE(k.contains("key"));
    }

    auto _ = keys.revoke(result["id"].get<std::string>(), "mb_admins", "shown_once_user");
}
