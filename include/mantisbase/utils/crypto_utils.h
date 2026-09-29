/**
 * @file crypto_utils.h
 * @brief Cryptographic helpers for OAuth PKCE, API keys, and token encryption.
 *
 * Wraps WolfSSL-backed primitives used by @ref OAuthManager and related auth flows.
 */

#ifndef MANTISBASE_CRYPTO_UTILS_H
#define MANTISBASE_CRYPTO_UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <mantisbase/core/types.h>

namespace mb {
    /** @return Cryptographically secure random bytes encoded as a hex or raw string. */
    MANTISBASE_API std::string generateSecureRandom(size_t length);

    /** @return Lowercase hex SHA-256 digest of `input`. */
    MANTISBASE_API std::string sha256Hex(const std::string &input);

    /** Base64url encode without padding (JWT/PKCE safe alphabet). */
    MANTISBASE_API std::string base64UrlEncode(const std::vector<uint8_t> &data);
    MANTISBASE_API std::string base64UrlEncode(const std::string &data);

    /** Base64url decode; returns empty vector on invalid input. */
    MANTISBASE_API std::vector<uint8_t> base64UrlDecode(const std::string &encoded);

    /** @return PKCE code verifier (43–128 unreserved characters). */
    MANTISBASE_API std::string generatePKCEVerifier(size_t length = 43);

    /** @return PKCE S256 challenge (`BASE64URL(SHA256(verifier))`). */
    MANTISBASE_API std::string generatePKCEChallenge(const std::string &verifier);

    /** AES-256-GCM encrypt; ciphertext includes IV + tag. Key must be 32 bytes. */
    MANTISBASE_API std::string aes256GcmEncrypt(const std::string &plaintext, const std::string &key);

    /** AES-256-GCM decrypt counterpart to @ref aes256GcmEncrypt. */
    MANTISBASE_API std::string aes256GcmDecrypt(const std::string &ciphertext, const std::string &key);
} // mb

#endif // MANTISBASE_CRYPTO_UTILS_H
