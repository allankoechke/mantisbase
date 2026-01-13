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
#ifdef _WIN32
#include <stdlib.h>
#else
#include <cstdlib>
#endif
#include "../test_fixure.h"
#include "test_config.h"

namespace fs = std::filesystem;

/**
 * @brief Simplified GoogleTest Environment for MantisBase tests
 * 
 * This class handles:
 * - Server setup and startup in a background thread
 * - Clean shutdown with timeout protection
 * - Test directory cleanup
 */
class MantisBaseTestEnvironment : public ::testing::Environment {
public:
    ~MantisBaseTestEnvironment() {
        // Safety net: detach thread if still joinable
        if (serverThread.joinable()) {
            std::cerr << "[TestEnvironment] WARNING: Thread still joinable in destructor, detaching...\n";
            serverThread.detach();
        }
    }

    void SetUp() override {
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

        auto port = TestConfig::getTestPort();

        // Configure MantisBase
        mb::json args;
        args["database"] = "SQLITE";
        args["dataDir"] = dataDir;
        args["publicDir"] = publicDir;
        args["scriptsDir"] = scriptingDir;
        args["serve"] = {{"port", port}, {"host", "0.0.0.0"}};

        // Create MantisBase instance
        auto &tFix = TestFixture::instance(args);

        // Start server in background thread
        // std::cout << "[TestEnvironment] Starting server in background thread...\n";
        // server_running.store(true);
        // serverThread = std::thread([&tFix, this]() {
        //     try {
        //         auto &app = tFix.app();
        //         // auto res = app.run();
        //         // std::cout << "[TestEnvironment] Server thread: app.run() returned with code " << res << '\n';
        //     } catch (const std::exception &e) {
        //         std::cerr << "[TestEnvironment] Server thread error: " << e.what() << '\n';
        //     } catch (...) {
        //         std::cerr << "[TestEnvironment] Server thread error: Unknown exception\n";
        //     }
        //     server_running.store(false);
        // });

        // Wait for server to initialize
        // std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Wait for server to be ready (health check)
        // std::cout << "[TestEnvironment] Waiting for server to be ready...\n";
        // if (!tFix.waitForServer(30, 200)) {
        //     std::cerr << "[TestEnvironment] Server failed to start within timeout\n";
        //     shutdown();
        //     throw std::runtime_error("Server failed to start within timeout");
        // }

        // std::cout << "[TestEnvironment] Server is ready, tests can begin...\n";
    }

    void TearDown() override {
        std::cout << "[TestEnvironment] Tearing down test environment...\n";
        // shutdown();
        // std::cout << "[TestEnvironment] Test environment cleaned up.\n";
    }

private:
    void shutdown() {
        // Step 1: Stop the HTTP server immediately
        try {
            auto &app = TestFixture::app();
            if (app.router().server().is_running()) {
                std::cout << "[TestEnvironment] Stopping HTTP server...\n";
                app.router().close();
                std::cout << "[TestEnvironment] HTTP server stopped.\n";
            }
        } catch (const std::exception &e) {
            std::cerr << "[TestEnvironment] Error stopping server: " << e.what() << '\n';
        }

        // Step 2: Wait briefly for server thread, then detach if needed
        if (serverThread.joinable()) {
            std::cout << "[TestEnvironment] Waiting for server thread to exit...\n";
            
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
                    // Thread finished, safe to join
                    serverThread.join();
                    std::cout << "[TestEnvironment] Server thread joined successfully.\n";
                } else {
                    // Thread still running, detach to prevent hang
                    std::cerr << "[TestEnvironment] WARNING: Server thread still running, detaching to prevent hang...\n";
                    serverThread.detach();
                }
            }
        }

        // Step 3: Clean up MantisBase instance
        // Don't wait for this - if it hangs, we've already detached the server thread
        try {
            auto &app = TestFixture::app();
            app.close();
            std::cout << "[TestEnvironment] MantisBase instance closed.\n";
        } catch (const std::exception &e) {
            std::cerr << "[TestEnvironment] Error closing MantisBase: " << e.what() << '\n';
        }

        // Step 4: Clean up test directory
        if (!baseDir.empty()) {
            std::cout << "[TestEnvironment] Cleaning up test directory...\n";
            TestFixture::cleanDir(baseDir);
        }

        // Final safety check
        if (serverThread.joinable()) {
            std::cerr << "[TestEnvironment] WARNING: Thread still joinable, detaching...\n";
            serverThread.detach();
        }
    }

    fs::path baseDir;
    std::thread serverThread;
    std::atomic<bool> server_running{false};
};

#endif //MANTISBASE_TEST_ENVIRONMENT_H
