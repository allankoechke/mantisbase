#include "mantisbase/utils/crypto_utils.h"
#include "mantisbase/core/logger/logger.h"

#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/coding.h>

#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace mb {
    std::string generateSecureRandom(size_t length) {
        WC_RNG rng;
        if (wc_InitRng(&rng) != 0) {
            throw std::runtime_error("Failed to initialize RNG");
        }

        std::vector<uint8_t> buf(length);
        if (wc_RNG_GenerateBlock(&rng, buf.data(), static_cast<word32>(length)) != 0) {
            wc_FreeRng(&rng);
            throw std::runtime_error("Failed to generate random bytes");
        }
        wc_FreeRng(&rng);

        std::ostringstream oss;
        for (auto b : buf) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        }
        return oss.str();
    }

    std::string sha256Hex(const std::string &input) {
        Sha256 sha;
        if (wc_InitSha256(&sha) != 0) {
            throw std::runtime_error("Failed to initialize SHA-256");
        }

        wc_Sha256Update(&sha,
                        reinterpret_cast<const byte*>(input.data()),
                        static_cast<word32>(input.size()));

        byte hash[SHA256_DIGEST_SIZE];
        wc_Sha256Final(&sha, hash);

        std::ostringstream oss;
        for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
        }
        return oss.str();
    }

    std::string base64UrlEncode(const std::vector<uint8_t> &data) {
        word32 outLen = static_cast<word32>(data.size() * 2 + 4);
        std::vector<byte> encoded(outLen);

        if (Base64_Encode_NoNl(data.data(), static_cast<word32>(data.size()),
                               encoded.data(), &outLen) != 0) {
            throw std::runtime_error("Base64 encode failed");
        }

        std::string result(reinterpret_cast<char*>(encoded.data()), outLen);

        // Convert standard base64 to URL-safe
        for (auto &c : result) {
            if (c == '+') c = '-';
            else if (c == '/') c = '_';
        }
        // Remove padding
        while (!result.empty() && result.back() == '=') {
            result.pop_back();
        }
        return result;
    }

    std::string base64UrlEncode(const std::string &data) {
        std::vector<uint8_t> vec(data.begin(), data.end());
        return base64UrlEncode(vec);
    }

    std::vector<uint8_t> base64UrlDecode(const std::string &encoded) {
        std::string padded = encoded;
        for (auto &c : padded) {
            if (c == '-') c = '+';
            else if (c == '_') c = '/';
        }
        while (padded.size() % 4 != 0) {
            padded += '=';
        }

        word32 outLen = static_cast<word32>(padded.size());
        std::vector<byte> decoded(outLen);

        if (Base64_Decode(reinterpret_cast<const byte*>(padded.data()),
                         static_cast<word32>(padded.size()),
                         decoded.data(), &outLen) != 0) {
            throw std::runtime_error("Base64 decode failed");
        }
        decoded.resize(outLen);
        return decoded;
    }

    std::string generatePKCEVerifier(size_t length) {
        static const char charset[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
        const size_t charset_size = sizeof(charset) - 1;

        WC_RNG rng;
        if (wc_InitRng(&rng) != 0) {
            throw std::runtime_error("Failed to initialize RNG");
        }

        std::vector<uint8_t> buf(length);
        if (wc_RNG_GenerateBlock(&rng, buf.data(), static_cast<word32>(length)) != 0) {
            wc_FreeRng(&rng);
            throw std::runtime_error("Failed to generate random bytes");
        }
        wc_FreeRng(&rng);

        std::string verifier;
        verifier.reserve(length);
        for (size_t i = 0; i < length; i++) {
            verifier += charset[buf[i] % charset_size];
        }
        return verifier;
    }

    std::string generatePKCEChallenge(const std::string &verifier) {
        Sha256 sha;
        if (wc_InitSha256(&sha) != 0) {
            throw std::runtime_error("Failed to initialize SHA-256");
        }

        wc_Sha256Update(&sha,
                        reinterpret_cast<const byte*>(verifier.data()),
                        static_cast<word32>(verifier.size()));

        byte hash[SHA256_DIGEST_SIZE];
        wc_Sha256Final(&sha, hash);

        std::vector<uint8_t> hashVec(hash, hash + SHA256_DIGEST_SIZE);
        return base64UrlEncode(hashVec);
    }

    std::string aes256GcmEncrypt(const std::string &plaintext, const std::string &key) {
        if (key.size() < 32) {
            throw std::runtime_error("AES-256-GCM requires a 32-byte key");
        }

        const int IV_SIZE = 12;
        const int TAG_SIZE = 16;

        WC_RNG rng;
        if (wc_InitRng(&rng) != 0) {
            throw std::runtime_error("Failed to initialize RNG");
        }

        std::vector<byte> iv(IV_SIZE);
        wc_RNG_GenerateBlock(&rng, iv.data(), IV_SIZE);
        wc_FreeRng(&rng);

        Aes aes;
        std::memset(&aes, 0, sizeof(aes));

        if (wc_AesGcmSetKey(&aes,
                           reinterpret_cast<const byte*>(key.data()), 32) != 0) {
            throw std::runtime_error("Failed to set AES-GCM key");
        }

        std::vector<byte> ciphertext(plaintext.size());
        std::vector<byte> tag(TAG_SIZE);

        if (wc_AesGcmEncrypt(&aes,
                             ciphertext.data(),
                             reinterpret_cast<const byte*>(plaintext.data()),
                             static_cast<word32>(plaintext.size()),
                             iv.data(), IV_SIZE,
                             tag.data(), TAG_SIZE,
                             nullptr, 0) != 0) {
            throw std::runtime_error("AES-GCM encryption failed");
        }

        // Format: IV || ciphertext || tag
        std::string result;
        result.append(reinterpret_cast<const char*>(iv.data()), IV_SIZE);
        result.append(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
        result.append(reinterpret_cast<const char*>(tag.data()), TAG_SIZE);

        return base64UrlEncode(result);
    }

    std::string aes256GcmDecrypt(const std::string &ciphertext_b64, const std::string &key) {
        if (key.size() < 32) {
            throw std::runtime_error("AES-256-GCM requires a 32-byte key");
        }

        const int IV_SIZE = 12;
        const int TAG_SIZE = 16;

        auto raw = base64UrlDecode(ciphertext_b64);
        if (raw.size() < static_cast<size_t>(IV_SIZE + TAG_SIZE)) {
            throw std::runtime_error("Invalid ciphertext: too short");
        }

        const byte* iv = raw.data();
        size_t ct_len = raw.size() - IV_SIZE - TAG_SIZE;
        const byte* ct = raw.data() + IV_SIZE;
        const byte* tag = raw.data() + IV_SIZE + ct_len;

        Aes aes;
        std::memset(&aes, 0, sizeof(aes));

        if (wc_AesGcmSetKey(&aes,
                           reinterpret_cast<const byte*>(key.data()), 32) != 0) {
            throw std::runtime_error("Failed to set AES-GCM key");
        }

        std::vector<byte> plaintext(ct_len);
        if (wc_AesGcmDecrypt(&aes,
                             plaintext.data(),
                             ct, static_cast<word32>(ct_len),
                             iv, IV_SIZE,
                             tag, TAG_SIZE,
                             nullptr, 0) != 0) {
            throw std::runtime_error("AES-GCM decryption failed: invalid key or tampered data");
        }

        return {reinterpret_cast<const char*>(plaintext.data()), plaintext.size()};
    }
} // mb
