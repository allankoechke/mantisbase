#include <gtest/gtest.h>
#include "mantisbase/utils/crypto_utils.h"
#include "mantisbase/mantisbase.h"
#include "../common/test_environment.h"
#include "../common/test_config.h"

class OAuthTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        app = &mb::MantisBase::instance();
    }

    mb::MantisBase* app = nullptr;
};

TEST_F(OAuthTestFixture, PKCEVerifierGeneration) {
    auto verifier = mb::generatePKCEVerifier();
    EXPECT_GE(verifier.size(), 43u);

    for (char c : verifier) {
        bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
        EXPECT_TRUE(valid) << "Invalid PKCE verifier character: " << c;
    }
}

TEST_F(OAuthTestFixture, PKCEChallengeGeneration) {
    auto verifier = mb::generatePKCEVerifier();
    auto challenge = mb::generatePKCEChallenge(verifier);

    EXPECT_FALSE(challenge.empty());
    EXPECT_NE(challenge, verifier);

    // Verify same verifier produces same challenge
    auto challenge2 = mb::generatePKCEChallenge(verifier);
    EXPECT_EQ(challenge, challenge2);
}

TEST_F(OAuthTestFixture, PKCEDifferentVerifiersDifferentChallenges) {
    auto verifier1 = mb::generatePKCEVerifier();
    auto verifier2 = mb::generatePKCEVerifier();

    EXPECT_NE(verifier1, verifier2);

    auto challenge1 = mb::generatePKCEChallenge(verifier1);
    auto challenge2 = mb::generatePKCEChallenge(verifier2);

    EXPECT_NE(challenge1, challenge2);
}

TEST_F(OAuthTestFixture, SHA256HashConsistency) {
    auto hash1 = mb::sha256Hex("test_input");
    auto hash2 = mb::sha256Hex("test_input");
    EXPECT_EQ(hash1, hash2);

    auto hash3 = mb::sha256Hex("different_input");
    EXPECT_NE(hash1, hash3);
}

TEST_F(OAuthTestFixture, SHA256HashLength) {
    auto hash = mb::sha256Hex("hello");
    EXPECT_EQ(hash.size(), 64u);
}

TEST_F(OAuthTestFixture, Base64UrlRoundTrip) {
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

TEST_F(OAuthTestFixture, SecureRandomUniqueness) {
    auto rand1 = mb::generateSecureRandom(32);
    auto rand2 = mb::generateSecureRandom(32);

    EXPECT_EQ(rand1.size(), 64u);
    EXPECT_EQ(rand2.size(), 64u);
    EXPECT_NE(rand1, rand2);
}

TEST_F(OAuthTestFixture, AES256GCMRoundTrip) {
    std::string key = "12345678901234567890123456789012"; // 32 bytes
    std::string plaintext = "sensitive OAuth token data";

    auto encrypted = mb::aes256GcmEncrypt(plaintext, key);
    EXPECT_FALSE(encrypted.empty());
    EXPECT_NE(encrypted, plaintext);

    auto decrypted = mb::aes256GcmDecrypt(encrypted, key);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(OAuthTestFixture, AES256GCMWrongKey) {
    std::string key1 = "12345678901234567890123456789012";
    std::string key2 = "abcdefghijklmnopqrstuvwxyz123456";
    std::string plaintext = "sensitive data";

    auto encrypted = mb::aes256GcmEncrypt(plaintext, key1);

    EXPECT_THROW(mb::aes256GcmDecrypt(encrypted, key2), std::runtime_error);
}

TEST_F(OAuthTestFixture, AES256GCMShortKeyFails) {
    std::string short_key = "too_short";
    std::string plaintext = "test";

    EXPECT_THROW(mb::aes256GcmEncrypt(plaintext, short_key), std::runtime_error);
}

TEST_F(OAuthTestFixture, AES256GCMDifferentCiphertexts) {
    std::string key = "12345678901234567890123456789012";
    std::string plaintext = "same plaintext";

    auto encrypted1 = mb::aes256GcmEncrypt(plaintext, key);
    auto encrypted2 = mb::aes256GcmEncrypt(plaintext, key);

    // Due to random IV, same plaintext produces different ciphertexts
    EXPECT_NE(encrypted1, encrypted2);

    // But both decrypt to the same plaintext
    EXPECT_EQ(mb::aes256GcmDecrypt(encrypted1, key), plaintext);
    EXPECT_EQ(mb::aes256GcmDecrypt(encrypted2, key), plaintext);
}
