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
    // Use temp_directory_path() consistently for cross-platform compatibility
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

struct TestFixture {
    int port;
    mb::MantisBase &mApp;
    static std::atomic<bool> server_ready;
    static std::mutex server_mutex;

private:
    explicit TestFixture(const mb::json &config)
        : port(TestConfig::getTestPort()),
          mApp(mb::MantisBase::create(config)) {
        std::cout << "[TestFixture] Setting up DB and starting server on port " << port << "...\n";
    }

public:
    static TestFixture &instance(const mb::json &config) {
        static TestFixture _instance{config};
        return _instance;
    }

    static mb::MantisBase &app() {
        return mb::MantisBase::instance();
    }

    // Wait until the server port responds with exponential backoff
    [[nodiscard]] bool waitForServer(const int max_retries = 20, const int initial_delay_ms = 50) const {
        // If server is already ready, return immediately
        if (server_ready.load()) {
            return true;
        }

        int delay_ms = initial_delay_ms;

        std::cout << "[TestFixture] Waiting for server on port " << port << "...\n";

        for (int i = 0; i < max_retries; ++i) {
            try {
                // Create a fresh client for each attempt to avoid connection issues
                // Use 127.0.0.1 instead of localhost for better reliability (per httplib docs)
                httplib::Client cli("127.0.0.1", port);
                cli.set_connection_timeout(1, 0); // 1 second connection timeout
                cli.set_read_timeout(1, 0); // 1 second read timeout

                // Try health endpoint first
                if (auto res = cli.Get("/api/v1/health")) {
                    if (res->status == 200) {
                        server_ready.store(true);
                        std::cout << "[TestFixture] Health check passed (attempt " << (i + 1) << ")\n";
                        return true;
                    } else {
                        if (i % 5 == 0) {
                            // Only log every 5th attempt to reduce noise
                            std::cout << "[TestFixture] Health check returned status " << res->status
                                    << " (attempt " << (i + 1) << ")\n";
                        }
                    }
                } else {
                    // Health endpoint failed, but try a fallback - check if server responds to any request
                    // This handles cases where health endpoint has issues but server is running
                    if (auto fallback_res = cli.Get("/mb")) {
                        // Server is responding (even if not 200, any response means server is up)
                        server_ready.store(true);
                        std::cout << "[TestFixture] Server is responding (fallback check passed, attempt " << (i + 1) <<
                                ")\n";
                        return true;
                    }

                    if (i % 5 == 0) {
                        // Only log every 5th attempt
                        std::cout << "[TestFixture] Health check failed - no response (attempt " << (i + 1) << ")\n";
                    }
                }
            } catch (const std::exception &e) {
                if (i % 5 == 0) {
                    // Only log every 5th attempt
                    std::cout << "[TestFixture] Health check exception: " << e.what()
                            << " (attempt " << (i + 1) << ")\n";
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            // Exponential backoff, max 500ms
            delay_ms = std::min(delay_ms * 2, 500);
        }

        std::cerr << "[TestFixture] Health check failed after " << max_retries << " attempts\n";
        return false;
    }

    static void teardownOnce() {
        std::lock_guard<std::mutex> lock(server_mutex);
        std::cout << "[TestFixture] Shutting down server ...\n";

        // First, just stop the HTTP server without destroying the router
        // This allows the server thread to exit cleanly from router.listen()
        try {
            if (app().router().server().is_running()) {
                app().router().close();
                std::cout << "[TestFixture] HTTP server stopped.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[TestFixture] Error stopping HTTP server: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[TestFixture] Unknown error stopping HTTP server\n";
        }

        std::cout << "[TestFixture] Server stopped.\n";
    }
    
    static void cleanup() {
        std::lock_guard<std::mutex> lock(server_mutex);
        // Now do the full cleanup - destroy router and other resources
        // This should only be called after the server thread has exited
        app().close();
    }


    static void cleanDir(const fs::path &base_path) {
        try {
            // Cleanup the temporary directory & files
            if (fs::exists(base_path)) {
                fs::remove_all(base_path);
                std::cout << "[TestFixture] Removed directory " << base_path.string() << "...\n";
            }
        } catch (const std::exception &e) {
            std::cerr << "[TestFixture] Cleaning up failed: " << e.what() << std::endl;
        }

        server_ready.store(false);
    }


    [[nodiscard]] httplib::Client client() const {
        return httplib::Client("127.0.0.1", port);
    }

    static httplib::Client staticClient() {
        return httplib::Client("127.0.0.1", TestConfig::getTestPort());
    }
};

#endif //MANTISBASE_TEST_FIXTURE_H
