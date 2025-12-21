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
 * @brief GoogleTest Environment to set up MantisBase for all tests (unit and integration)
 * 
 * This ensures MantisBase is initialized once before all tests run,
 * and starts the server in a separate thread for integration tests.
 */
class MantisBaseTestEnvironment : public ::testing::Environment {
public:
    ~MantisBaseTestEnvironment() {
        // Safety net: Ensure thread is cleaned up even if TearDown wasn't called or threw
        // This should rarely be needed since TearDown should handle cleanup
        if (serverThread.joinable()) {
            // Don't call teardownOnce() here as it may have already been called
            // Just detach to avoid std::terminate
            std::cerr << "[TestEnvironment] WARNING: Thread still joinable in destructor, detaching to avoid crash.\n";
            serverThread.detach();
        }
    }
    
    void SetUp() override {
        // Disable rate limiting in tests to prevent test failures
#ifdef _WIN32
        _putenv_s("MB_DISABLE_RATE_LIMIT", "1");
        _putenv_s("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1");
#else
        setenv("MB_DISABLE_RATE_LIMIT", "1", 1);
        setenv("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1", 1);
#endif

        // Setup directories for tests, for each, create a unique directory
        // in which we will use for current tests then later tear it down.
        baseDir = getBaseDir();
        const auto scriptingDir = (fs::path(TEST_SOURCE_DIR) / "scripting").string();
        const auto dataDir = (baseDir / "data").string();
        const auto publicDir = (baseDir / "www").string();

        auto port = TestConfig::getTestPort();

        mb::json args;
        // args["dev"] = true;
        args["database"] = "SQLITE";
        args["dataDir"] = dataDir;
        args["publicDir"] = publicDir;
        args["scriptsDir"] = scriptingDir;
        args["serve"] = {{"port", port}, {"host", "0.0.0.0"}};

        // mb::logger::trace("Test Environment Args: {}", args.dump());

        // Setup MantisBase instance
        auto& tFix = TestFixture::instance(args);

        // Spawn a new thread to run the server
        server_error.store(false);
        {
            std::lock_guard<std::mutex> lock(server_error_mutex);
            server_error_msg.clear();
        }
        
        std::cout << "[TestEnvironment] Starting server thread...\n";
        serverThread = std::thread([&tFix, this]()
        {
            try
            {
                auto& app = tFix.app();
                [[maybe_unused]] auto res = app.run();
                std::cout << "[TestEnvironment] Server thread: app.run() returned with code " << res << '\n';
            }
            catch (const std::exception& e)
            {
                server_error.store(true);
                {
                    std::lock_guard<std::mutex> lock(server_error_mutex);
                    server_error_msg = e.what();
                }
                std::cerr << "[TestEnvironment] Error Starting Server: " << e.what() << '\n';
            }
            catch (...)
            {
                server_error.store(true);
                {
                    std::lock_guard<std::mutex> lock(server_error_mutex);
                    server_error_msg = "Unknown error";
                }
                std::cerr << "[TestEnvironment] Error Starting Server: Unknown error\n";
            }
        });

        // Give the server thread time to initialize and start listening
        // Router initialization queries the database which can take a moment
        std::cout << "[TestEnvironment] Waiting for server to initialize...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (server_error.load()) {
            std::string error_msg;
            {
                std::lock_guard<std::mutex> lock(server_error_mutex);
                error_msg = server_error_msg;
            }
            std::cerr << "[TestEnvironment] Server thread failed: " << error_msg << '\n';
            if (serverThread.joinable()) {
                serverThread.join();
            }
            TestFixture::teardownOnce();
            TestFixture::cleanDir(baseDir);
            throw std::runtime_error("Failed to start test server: " + error_msg);
        }

        // Wait for server to be ready (health check)
        if (!tFix.waitForServer(30, 200)) {
            std::cerr << "[TestEnvironment] Server failed to start within timeout\n";
            if (serverThread.joinable()) {
                serverThread.join();
            }
            TestFixture::teardownOnce();
            TestFixture::cleanDir(baseDir);
            throw std::runtime_error("Server failed to start within timeout");
        }
        
        std::cout << "[TestEnvironment] Server is ready, tests can begin...\n";
    }

    void TearDown() override {
        std::cout << "[TestEnvironment] Tearing down test environment...\n";
        
        // First, stop just the HTTP server (without destroying router)
        // This allows the server thread to exit cleanly
        std::cout << "[TestEnvironment] Stopping HTTP server...\n";
        TestFixture::teardownOnce();
        std::cout << "[TestEnvironment] HTTP server stop command sent.\n";
        
        // Wait for server thread to finish - give it time to exit from app.run()
        if (serverThread.joinable()) {
            std::cout << "[TestEnvironment] Waiting for server thread to exit...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            std::cout << "[TestEnvironment] Attempting to join server thread...\n";
            if (serverThread.joinable()) {
                serverThread.join();
                std::cout << "[TestEnvironment] Server thread joined successfully.\n";
            }
        }
        
        // Now that the thread has exited, do full cleanup (destroy router, etc.)
        std::cout << "[TestEnvironment] Performing full cleanup...\n";
        TestFixture::cleanup();
        std::cout << "[TestEnvironment] Full cleanup completed.\n";
        
        // Clean up test directory
        std::cout << "[TestEnvironment] Cleaning up test directory...\n";
        if (!baseDir.empty()) {
            TestFixture::cleanDir(baseDir);
        }
        
        // Final safety check - ensure thread is not joinable
        if (serverThread.joinable()) {
            serverThread.detach();
        }
        
        std::cout << "[TestEnvironment] Test environment cleaned up.\n";

        // Hack to avoid post actions on the test cleanup that trigger
        // SIGABORT which results in failed tests
        std::exit(0);
    }

private:
    fs::path baseDir;
    std::thread serverThread;
    std::atomic<bool> server_error{false};
    std::mutex server_error_mutex;
    std::string server_error_msg;
};

#endif //MANTISBASE_TEST_ENVIRONMENT_H

