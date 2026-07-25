#include <gtest/gtest.h>
#include <soci/soci.h>
#include "mantisbase/utils/crypto_utils.h"
#include "mantisbase/core/oauth.h"
#include "mantisbase/mantisbase.h"
#include "../common/test_fixture.h"
#include "../common/test_config.h"

TEST(OAuthCrypto, PKCEVerifierGeneration) {
    auto verifier = mb::generatePKCEVerifier();
    EXPECT_GE(verifier.size(), 43u);

    for (char c : verifier) {
        bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
        EXPECT_TRUE(valid) << "Invalid PKCE verifier character: " << c;
    }
}

TEST(OAuthCrypto, PKCEChallengeGeneration) {
    auto verifier = mb::generatePKCEVerifier();
    auto challenge = mb::generatePKCEChallenge(verifier);

    EXPECT_FALSE(challenge.empty());
    EXPECT_NE(challenge, verifier);

    auto challenge2 = mb::generatePKCEChallenge(verifier);
    EXPECT_EQ(challenge, challenge2);
}

TEST(OAuthCrypto, PKCEDifferentVerifiersDifferentChallenges) {
    auto verifier1 = mb::generatePKCEVerifier();
    auto verifier2 = mb::generatePKCEVerifier();

    EXPECT_NE(verifier1, verifier2);

    auto challenge1 = mb::generatePKCEChallenge(verifier1);
    auto challenge2 = mb::generatePKCEChallenge(verifier2);

    EXPECT_NE(challenge1, challenge2);
}

TEST(OAuthCrypto, SHA256HashConsistency) {
    auto hash1 = mb::sha256Hex("test_input");
    auto hash2 = mb::sha256Hex("test_input");
    EXPECT_EQ(hash1, hash2);

    auto hash3 = mb::sha256Hex("different_input");
    EXPECT_NE(hash1, hash3);
}

TEST(OAuthCrypto, SHA256HashLength) {
    auto hash = mb::sha256Hex("hello");
    EXPECT_EQ(hash.size(), 64u);
}

TEST(OAuthCrypto, Base64UrlRoundTrip) {
    std::string original = "Hello, World! This is a test.";
    auto encoded = mb::base64UrlEncode(original);

    EXPECT_FALSE(encoded.empty());
    EXPECT_EQ(encoded.find('+'), std::string::npos);
    EXPECT_EQ(encoded.find('/'), std::string::npos);
    EXPECT_EQ(encoded.find('='), std::string::npos);

    auto decoded = mb::base64UrlDecode(encoded);
    std::string result(decoded.begin(), decoded.end());
    EXPECT_EQ(result, original);
}

TEST(OAuthCrypto, SecureRandomUniqueness) {
    auto rand1 = mb::generateSecureRandom(32);
    auto rand2 = mb::generateSecureRandom(32);

    EXPECT_EQ(rand1.size(), 64u);
    EXPECT_EQ(rand2.size(), 64u);
    EXPECT_NE(rand1, rand2);
}

TEST(OAuthCrypto, AES256GCMRoundTrip) {
    std::string key = "12345678901234567890123456789012";
    std::string plaintext = "sensitive OAuth token data";

    auto encrypted = mb::aes256GcmEncrypt(plaintext, key);
    EXPECT_FALSE(encrypted.empty());
    EXPECT_NE(encrypted, plaintext);

    auto decrypted = mb::aes256GcmDecrypt(encrypted, key);
    EXPECT_EQ(decrypted, plaintext);
}

TEST(OAuthCrypto, AES256GCMWrongKey) {
    std::string key1 = "12345678901234567890123456789012";
    std::string key2 = "abcdefghijklmnopqrstuvwxyz123456";
    std::string plaintext = "sensitive data";

    auto encrypted = mb::aes256GcmEncrypt(plaintext, key1);

    EXPECT_THROW(mb::aes256GcmDecrypt(encrypted, key2), std::runtime_error);
}

TEST(OAuthCrypto, AES256GCMShortKeyFails) {
    std::string short_key = "too_short";
    std::string plaintext = "test";

    EXPECT_THROW(mb::aes256GcmEncrypt(plaintext, short_key), std::runtime_error);
}

TEST(OAuthCrypto, AES256GCMDifferentCiphertexts) {
    std::string key = "12345678901234567890123456789012";
    std::string plaintext = "same plaintext";

    auto encrypted1 = mb::aes256GcmEncrypt(plaintext, key);
    auto encrypted2 = mb::aes256GcmEncrypt(plaintext, key);

    EXPECT_NE(encrypted1, encrypted2);

    EXPECT_EQ(mb::aes256GcmDecrypt(encrypted1, key), plaintext);
    EXPECT_EQ(mb::aes256GcmDecrypt(encrypted2, key), plaintext);
}

class OAuthDbTest : public MbAppFixture {
};

TEST_F(OAuthDbTest, HandleCallbackRejectsInvalidState) {
    EXPECT_THROW(
        mantis().auth().oauth().handleCallback("test_entity", "google", "fake_code", "invalid_state_value"),
        std::runtime_error
    );
}

TEST_F(OAuthDbTest, HandleCallbackRejectsExpiredState) {
    auto sql = mantis().db().session();
    auto state_id = mb::generateTimeBasedId();
    std::string state = mb::generateSecureRandom(32);
    std::string verifier = mb::generatePKCEVerifier();

    *sql << "INSERT INTO mb_oauth_states (id, state, pkce_verifier, entity_name, provider_id, redirect_uri, expires_at) "
            "VALUES (:id, :state, :verifier, 'test_entity', 'fake_provider', 'http://localhost/cb', datetime('now', '-1 hour'))",
        soci::use(state_id), soci::use(state), soci::use(verifier);

    EXPECT_THROW(
        mantis().auth().oauth().handleCallback("test_entity", "google", "fake_code", state),
        std::runtime_error
    );
}
