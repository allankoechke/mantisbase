#ifndef MANTISBASE_CRYPTO_UTILS_H
#define MANTISBASE_CRYPTO_UTILS_H

#include <string>
#include <vector>
#include <cstdint>

namespace mb {
    std::string generateSecureRandom(size_t length);

    std::string sha256Hex(const std::string &input);

    std::string base64UrlEncode(const std::vector<uint8_t> &data);
    std::string base64UrlEncode(const std::string &data);
    std::vector<uint8_t> base64UrlDecode(const std::string &encoded);

    std::string generatePKCEVerifier(size_t length = 43);
    std::string generatePKCEChallenge(const std::string &verifier);

    std::string aes256GcmEncrypt(const std::string &plaintext, const std::string &key);
    std::string aes256GcmDecrypt(const std::string &ciphertext, const std::string &key);
} // mb

#endif // MANTISBASE_CRYPTO_UTILS_H
