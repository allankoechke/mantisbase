#include <gtest/gtest.h>
#include "mantisbase/core/api_keys.h"
#include "mantisbase/utils/crypto_utils.h"
#include "mantisbase/mantisbase.h"
#include "../common/test_environment.h"
#include "../common/test_config.h"

class ApiKeyTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        app = &mb::MantisBase::instance();
    }

    mb::MantisBase* app = nullptr;
};

TEST_F(ApiKeyTestFixture, GenerateApiKey) {
    auto result = mb::ApiKeyManager::generateApiKey();

    EXPECT_FALSE(result.id.empty());
    EXPECT_TRUE(result.key.starts_with("mb_sk_"));
    EXPECT_FALSE(result.key_hash.empty());
    EXPECT_EQ(result.key_hash.size(), 64u);
}

TEST_F(ApiKeyTestFixture, GenerateUniqueKeys) {
    auto key1 = mb::ApiKeyManager::generateApiKey();
    auto key2 = mb::ApiKeyManager::generateApiKey();

    EXPECT_NE(key1.key, key2.key);
    EXPECT_NE(key1.key_hash, key2.key_hash);
    EXPECT_NE(key1.id, key2.id);
}

TEST_F(ApiKeyTestFixture, HashApiKeyConsistency) {
    std::string raw_key = "mb_sk_abcdef1234567890abcdef1234567890abcdef1234567890abcdef12345678";
    auto hash1 = mb::ApiKeyManager::hashApiKey(raw_key);
    auto hash2 = mb::ApiKeyManager::hashApiKey(raw_key);

    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.size(), 64u);
}

TEST_F(ApiKeyTestFixture, DifferentKeysDifferentHashes) {
    std::string key1 = "mb_sk_aaaa";
    std::string key2 = "mb_sk_bbbb";

    auto hash1 = mb::ApiKeyManager::hashApiKey(key1);
    auto hash2 = mb::ApiKeyManager::hashApiKey(key2);

    EXPECT_NE(hash1, hash2);
}

TEST_F(ApiKeyTestFixture, CreateAndLookupApiKey) {
    auto result = mb::ApiKeyManager::create("mb_admins", "test_user_id", "Test Key");

    EXPECT_TRUE(result.contains("id"));
    EXPECT_TRUE(result.contains("key"));
    EXPECT_TRUE(result["key"].get<std::string>().starts_with("mb_sk_"));
    EXPECT_EQ(result["label"].get<std::string>(), "Test Key");

    // Lookup by hash
    auto key_hash = mb::ApiKeyManager::hashApiKey(result["key"].get<std::string>());
    auto lookup = mb::ApiKeyManager::lookupByHash(key_hash);

    EXPECT_TRUE(lookup.has_value());
    EXPECT_EQ(lookup.value()["user_id"].get<std::string>(), "test_user_id");
    EXPECT_EQ(lookup.value()["entity_name"].get<std::string>(), "mb_admins");
}

TEST_F(ApiKeyTestFixture, LookupNonExistentKey) {
    auto result = mb::ApiKeyManager::lookupByHash("nonexistent_hash_value_that_does_not_exist");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ApiKeyTestFixture, CreateListAndRevokeApiKey) {
    auto created = mb::ApiKeyManager::create("mb_admins", "list_test_user", "List Test Key");
    auto key_id = created["id"].get<std::string>();

    // List keys for this user
    auto keys = mb::ApiKeyManager::list("mb_admins", "list_test_user");
    EXPECT_GE(keys.size(), 1u);

    bool found = false;
    for (const auto& k : keys) {
        if (k["id"].get<std::string>() == key_id) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    // Revoke
    bool revoked = mb::ApiKeyManager::revoke(key_id, "mb_admins", "list_test_user");
    EXPECT_TRUE(revoked);

    // Lookup should fail after revocation
    auto key_hash = mb::ApiKeyManager::hashApiKey(created["key"].get<std::string>());
    auto lookup = mb::ApiKeyManager::lookupByHash(key_hash);
    EXPECT_FALSE(lookup.has_value());
}

TEST_F(ApiKeyTestFixture, RevokeNonExistentKey) {
    bool revoked = mb::ApiKeyManager::revoke("nonexistent_id", "mb_admins", "no_user");
    EXPECT_FALSE(revoked);
}

TEST_F(ApiKeyTestFixture, ShownOnceVerification) {
    auto result = mb::ApiKeyManager::create("mb_admins", "shown_once_user", "Shown Once Key");

    // The raw key is returned only at creation time
    EXPECT_TRUE(result.contains("key"));
    auto raw_key = result["key"].get<std::string>();
    EXPECT_TRUE(raw_key.starts_with("mb_sk_"));

    // Listing keys does NOT return the raw key
    auto keys = mb::ApiKeyManager::list("mb_admins", "shown_once_user");
    for (const auto& k : keys) {
        EXPECT_FALSE(k.contains("key"));
    }

    // Clean up
    mb::ApiKeyManager::revoke(result["id"].get<std::string>(), "mb_admins", "shown_once_user");
}
