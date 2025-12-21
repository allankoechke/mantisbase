#ifndef MANTISBASE_TEST_CONFIG_H
#define MANTISBASE_TEST_CONFIG_H

#include <string>
#include "../include/mantisbase/utils/utils.h"

namespace TestConfig {
    /**
     * @brief Get test password, using environment variable or generating random one
     */
    inline std::string getTestPassword() {
        static std::string pwd = []() {
            auto env_pwd = mb::getEnvOrDefault("TEST_PASSWORD", "");
            if (!env_pwd.empty()) {
                return env_pwd;
            }
            // Generate random password for tests
            return "test_" + mb::generateShortId(12);
        }();
        return pwd;
    }
    
    /**
     * @brief Get test port, using environment variable or default
     */
    inline int getTestPort() {
        static int port = []() {
            auto env_port = mb::getEnvOrDefault("TEST_PORT", "");
            if (!env_port.empty()) {
                return std::stoi(env_port);
            }
            return 7075; // Default test port
        }();
        return port;
    }
    
    /**
     * @brief Check if rate limiting should be disabled in tests
     */
    inline bool isRateLimitingDisabled() {
        static bool disabled = mb::getEnvOrDefault("MB_DISABLE_RATE_LIMIT", "0") == "1";
        return disabled;
    }
    
    /**
     * @brief Get admin email for tests
     */
    inline std::string getAdminEmail() {
        return mb::getEnvOrDefault("MB_TEST_ADMIN_EMAIL", "admin@test.com");
    }
    
    /**
     * @brief Get test user email prefix
     */
    inline std::string getTestUserEmailPrefix() {
        return mb::getEnvOrDefault("TEST_USER_EMAIL_PREFIX", "testuser");
    }
}

#endif //MANTISBASE_TEST_CONFIG_H

