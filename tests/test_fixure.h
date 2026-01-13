#ifndef MANTISBASE_TEST_FIXTURE_H
#define MANTISBASE_TEST_FIXTURE_H

#include <thread>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <utility>

#include "../include/mantisbase/mantis.h"
#include "common/test_config.h"

namespace fs = std::filesystem;

inline fs::path getBaseDir() {
    // Base test directory for files and SQLite data
    auto base_path = fs::temp_directory_path() / "mantisbase_tests" / mb::generateShortId();

    try {
        fs::create_directories(base_path);
        // Set restrictive permissions (owner read/write/execute only)
        fs::permissions(base_path, fs::perms::owner_all, fs::perm_options::replace);
    } catch (const std::exception &e) {
        std::cerr << "[FS Create] " << e.what() << std::endl;
    }

    return base_path;
}

/**
 * @brief Simplified test fixture for accessing MantisBase instance
 */
struct TestFixture {
    int port;
    mb::MantisBase &mApp;
    static std::atomic<bool> server_ready;

private:
    explicit TestFixture(const mb::json &config)
        : port(TestConfig::getTestPort()),
          mApp(mb::MantisBase::create(config)) {
    }

public:
    static TestFixture &instance(const mb::json &config) {
        static TestFixture _instance{config};
        return _instance;
    }

    static mb::MantisBase &app() {
        return mb::MantisBase::instance();
    }

    /**
     * @brief Wait until the server port responds with exponential backoff
     */
    [[nodiscard]] bool waitForServer(const int max_retries = 20, const int initial_delay_ms = 50) const {
        if (server_ready.load()) {
            return true;
        }

        int delay_ms = initial_delay_ms;
        std::cout << "[TestFixture] Waiting for server on port " << port << "...\n";

        for (int i = 0; i < max_retries; ++i) {
            try {
                httplib::Client cli("127.0.0.1", port);
                cli.set_connection_timeout(1, 0);
                cli.set_read_timeout(1, 0);

                // Try health endpoint
                if (auto res = cli.Get("/api/v1/health")) {
                    if (res->status == 200) {
                        server_ready.store(true);
                        std::cout << "[TestFixture] Health check passed (attempt " << (i + 1) << ")\n";
                        return true;
                    }
                } else {
                    // Fallback: check if server responds at all
                    if (auto fallback_res = cli.Get("/mb")) {
                        server_ready.store(true);
                        std::cout << "[TestFixture] Server is responding (attempt " << (i + 1) << ")\n";
                        return true;
                    }
                }
            } catch (const std::exception &e) {
                if (i % 5 == 0) {
                    std::cout << "[TestFixture] Health check exception: " << e.what()
                            << " (attempt " << (i + 1) << ")\n";
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms = std::min(delay_ms * 2, 500); // Exponential backoff, max 500ms
        }

        std::cerr << "[TestFixture] Health check failed after " << max_retries << " attempts\n";
        return false;
    }

    /**
     * @brief Clean up test directory
     */
    static void cleanDir(const fs::path &base_path) {
        try {
            if (fs::exists(base_path)) {
                fs::remove_all(base_path);
                std::cout << "[TestFixture] Removed directory " << base_path.string() << "...\n";
            }
        } catch (const std::exception &e) {
            std::cerr << "[TestFixture] Cleaning up failed: " << e.what() << std::endl;
        }
        server_ready.store(false);
    }

    /**
     * @brief Get HTTP client for testing
     */
    [[nodiscard]] httplib::Client client() const {
        return httplib::Client("127.0.0.1", port);
    }

    /**
     * @brief Get static HTTP client (uses configured test port)
     */
    static httplib::Client staticClient() {
        return httplib::Client("127.0.0.1", TestConfig::getTestPort());
    }
};

#endif //MANTISBASE_TEST_FIXTURE_H
