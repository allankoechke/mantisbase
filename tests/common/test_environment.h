#ifndef MANTISBASE_TEST_ENVIRONMENT_H
#define MANTISBASE_TEST_ENVIRONMENT_H

#include <gtest/gtest.h>
#include <filesystem>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <mutex>
#include <utility>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <cstdlib>
#endif
#include "../include/mantisbase/mantis.h"
#include "test_config.h"

namespace fs = std::filesystem;

/**
 * @brief Unified test environment for MantisBase tests
 * 
 * This class handles:
 * - GoogleTest environment setup/teardown
 * - Server setup and startup in a background thread
 * - Clean shutdown with timeout protection
 * - Test directory cleanup
 * - Access to MantisBase instance and HTTP clients
 */
class MbTestEnv : public ::testing::Environment {
public:
    MbTestEnv();

    // Constructor is public so GoogleTest can create the instance
    ~MbTestEnv() override;

    MbTestEnv(const MbTestEnv &) = delete;

    MbTestEnv &operator=(const MbTestEnv &) = delete;

    // GoogleTest Environment interface
    void SetUp() override;

    void TearDown() override;

    // Singleton access - returns the GoogleTest-managed instance
    static MbTestEnv &instance();

    // Access to MantisBase instance
    // static mb::MantisBase &app() {
    //     std::cout << "[MbTestEnv] MbTestEnv::app() called\n";
    //     std::cout.flush();
    //     return mb::MantisBase::instance();
    // }

    /**
     * @brief Wait until the server port responds with exponential backoff
     */
    [[nodiscard]] bool waitForServer(const int max_retries = 20, const int initial_delay_ms = 50) const;

    /**
     * @brief Get HTTP client for testing
     */
    [[nodiscard]] httplib::Client client() const;

    /**
     * @brief Get static HTTP client (uses configured test port)
     */
    static httplib::Client staticClient();

    /**
     * @brief Clean up test directory
     */
    static void cleanDir(const fs::path &base_path);

    // Get port
    [[nodiscard]] int getPort() const;

private:
    static MbTestEnv *&getInstancePtr();

    static fs::path getBaseDir();

    void shutdown();

    fs::path baseDir;
    std::thread serverThread;
    std::atomic<bool> server_running{false};
    int port{0};
    static std::atomic<bool> server_ready;
};

inline MbTestEnv::MbTestEnv() {
    // Register this instance for singleton access
    getInstancePtr() = this;

    // Disable rate limiting and admin setup prompts in tests
#ifdef _WIN32
    _putenv_s("MB_DISABLE_RATE_LIMIT", "1");
    _putenv_s("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1");
#else
    setenv("MB_DISABLE_RATE_LIMIT", "1", 1);
    setenv("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1", 1);
#endif

    // Create unique test directory
    baseDir = getBaseDir();
    const auto scriptingDir = (fs::path(TEST_SOURCE_DIR) / "scripting").string();
    const auto dataDir = (baseDir / "data").string();
    const auto publicDir = (baseDir / "www").string();

    port = TestConfig::getTestPort();

    // Configure MantisBase
    mb::json args;
    args["dev"] = true;
    args["database"] = "SQLITE";
    args["dataDir"] = dataDir;
    args["publicDir"] = publicDir;
    args["scriptsDir"] = scriptingDir;
    args["serve"] = {{"port", port}, {"host", "0.0.0.0"}};

    // Create MantisBase instance
    mb::MantisBase::create(args);
}

inline MbTestEnv::~MbTestEnv() = default;

inline void MbTestEnv::SetUp() {
    serverThread = std::thread([this]() {
        try {
            // Start server in background thread
            std::cout << "[MbTestEnv] Starting server in background thread...\n";
            server_running.store(true);
            auto res = mb::MantisBase::instance().run();
            std::cout << "[MbTestEnv] Server thread: app.run() returned with code " << res << '\n';
        } catch (const std::exception &e) {
            std::cerr << "[MbTestEnv] Server thread error: " << e.what() << '\n';
        }

        server_running.store(false);
    });

    // Wait for server to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Wait for server to be ready (health check)
    std::cout << "[MbTestEnv] Waiting for server to be ready...\n";
    if (!waitForServer(30, 200)) {
        std::cerr << "[MbTestEnv] Server failed to start within timeout\n";
        shutdown();
        return;
    }

    std::cout << "[MbTestEnv] Server is ready, tests can begin...\n";
}

inline void MbTestEnv::TearDown() {
    shutdown();
}

inline MbTestEnv &MbTestEnv::instance() {
    std::cout << "[MbTestEnv] MbTestEnv::instance1() called\n";
    std::cout.flush();

    MbTestEnv *inst = getInstancePtr();
    if (inst == nullptr) {
        std::cerr << "[MbTestEnv] MbTestEnv::instance() called but inst is nullptr\n";
        throw std::runtime_error("MbTestEnv::instance() called before SetUp()");
    }
    return *inst;
}

inline bool MbTestEnv::waitForServer(const int max_retries, const int initial_delay_ms) const {
    if (server_ready.load()) {
        return true;
    }

    int delay_ms = initial_delay_ms;
    for (int i = 0; i < max_retries; ++i) {
        try {
            httplib::Client cli("127.0.0.1", port);
            cli.set_connection_timeout(1, 0);
            cli.set_read_timeout(1, 0);

            // Try health endpoint
            if (auto res = cli.Get("/api/v1/health")) {
                if (res->status == 200) {
                    server_ready.store(true);
                    std::cout << "[MbTestEnv] Health check passed (attempt " << (i + 1) << ")\n";
                    return true;
                }
            } else {
                // Fallback: check if server responds at all
                if (auto fallback_res = cli.Get("/mb")) {
                    server_ready.store(true);
                    std::cout << "[MbTestEnv] Server is responding (attempt " << (i + 1) << ")\n";
                    return true;
                }
            }
        } catch (const std::exception &e) {
            if (i % 5 == 0) {
                std::cout << "[MbTestEnv] Health check exception: " << e.what()
                        << " (attempt " << (i + 1) << ")\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        delay_ms = std::min(delay_ms * 2, 500); // Exponential backoff, max 500ms
    }

    std::cerr << "[MbTestEnv] Health check failed after " << max_retries << " attempts\n";
    return false;
}

inline httplib::Client MbTestEnv::client() const {
    return httplib::Client("127.0.0.1", port);
}

inline httplib::Client MbTestEnv::staticClient() {
    return httplib::Client("127.0.0.1", TestConfig::getTestPort());
}

inline void MbTestEnv::cleanDir(const fs::path &base_path) {
    try {
        if (fs::exists(base_path)) {
            fs::remove_all(base_path);
            std::cout << "[MbTestEnv] Removed directory " << base_path.string() << "...\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "[MbTestEnv] Cleaning up failed: " << e.what() << std::endl;
    }
    server_ready.store(false);
}

inline int MbTestEnv::getPort() const {
    return port;
}

inline MbTestEnv *&MbTestEnv::getInstancePtr() {
    std::cout << "[MbTestEnv] MbTestEnv::getInstancePtr() called\n";
    std::cout.flush();
    static MbTestEnv *instance_ptr = nullptr;
    return instance_ptr;
}

inline fs::path MbTestEnv::getBaseDir() {
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

inline void MbTestEnv::shutdown() {
    std::cout << "[MbTestEnv] Test environment shutting down ...\n";
    try {
        // Stop the HTTP server immediately
        if (auto &router = mb::MantisBase::instance().router();
            router.server().is_running()) router.close();
    } catch (const std::exception &e) {
        std::cerr << "[MbTestEnv] Error stopping server: " << e.what() << '\n';
    }

    // Wait briefly for server thread, then detach if needed
    if (serverThread.joinable()) {
        std::cout << "[MbTestEnv] Waiting for server thread to exit...\n";

        // Give thread a short time to exit naturally
        const auto quick_timeout = std::chrono::milliseconds(500);
        const auto start = std::chrono::steady_clock::now();

        while (server_running.load() &&
               (std::chrono::steady_clock::now() - start) < quick_timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // If still running, detach immediately to avoid blocking
        if (serverThread.joinable()) {
            if (!server_running.load()) {
                serverThread.join();
                std::cout << "[MbTestEnv] Server thread joined successfully.\n";
            } else {
                // Thread still running, detach to prevent hang
                std::cerr << "[MbTestEnv] WARNING: Server thread still running, detaching to prevent hang...\n";
                serverThread.detach();
            }
        }
    }

    // Clean up test directory
    if (!baseDir.empty()) {
        std::cout << "[MbTestEnv] Cleaning up test directory...\n";
        cleanDir(baseDir);
    }

    // Final safety check
    if (serverThread.joinable()) {
        std::cerr << "[MbTestEnv] WARNING: Thread still joinable, detaching...\n";
        serverThread.detach();
    }
}

#endif //MANTISBASE_TEST_ENVIRONMENT_H
