#include <gtest/gtest.h>
#include <iostream>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <cstdlib>
#endif
#include "test_fixure.h"
#include "common/test_config.h"

int main(int argc, char* argv[])
{
    // Disable rate limiting in tests to prevent test failures
    #ifdef _WIN32
        _putenv_s("TEST_DISABLE_RATE_LIMIT", "1");
    #else
        setenv("TEST_DISABLE_RATE_LIMIT", "1", 1);
    #endif
    
    // Setup directories for tests, for each, create a unique directory
    // in which we will use for current tests then later tear it down.
    const auto baseDir = getBaseDir();
    const auto scriptingDir = (fs::path(TEST_SOURCE_DIR) / "scripting").string();
    const auto dataDir = (baseDir / "data").string();
    const auto publicDir = (baseDir / "www").string();

    auto port = TestConfig::getTestPort();

    mb::json args;
    args["database"] = "SQLITE";
    args["dataDir"] = dataDir;
    args["publicDir"] = publicDir;
    args["scriptsDir"] = scriptingDir;
    args["serve"] = {{"port", port}, {"host", "0.0.0.0"}};

    mb::logger::trace("Args: {}", args.dump());

    // Setup Db, Server, etc.
    auto& tFix = TestFixture::instance(args);

    // Spawn a new thread to run the server
    std::atomic<bool> server_error{false};
    std::string server_error_msg;
    
    std::cout << "[Test] Starting server thread...\n";
    auto serverThread = std::thread([&tFix, &server_error, &server_error_msg]()
    {
        try
        {
            std::cout << "[Test] Server thread: Calling app.run()...\n";
            auto& app = tFix.app();
            std::cout << "[Test] Server thread: app.run() started, server should be listening...\n";
            [[maybe_unused]] auto res = app.run();
            std::cout << "[Test] Server thread: app.run() returned with code " << res << '\n';
        }
        catch (const std::exception& e)
        {
            server_error.store(true);
            server_error_msg = e.what();
            std::cerr << "[Test] Error Starting Server: " << e.what() << '\n';
        }
        catch (...)
        {
            server_error.store(true);
            server_error_msg = "Unknown error";
            std::cerr << "[Test] Error Starting Server: Unknown error\n";
        }
    });

    // Give the server thread time to initialize and start listening
    // Router initialization queries the database which can take a moment
    std::cout << "[Test] Waiting for server to initialize...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (server_error.load()) {
        std::cerr << "[Test] Server thread failed: " << server_error_msg << '\n';
        if (serverThread.joinable()) {
            serverThread.join();
        }
        tFix.teardownOnce(baseDir);
        return 1;
    }

    // Wait for server to be ready (health check)
    std::cout << "[Test] Waiting for server to respond to health checks...\n";
    if (!tFix.waitForServer(30, 200)) {
        std::cerr << "[Test] Server failed to start within timeout\n";
        if (serverThread.joinable()) {
            serverThread.join();
        }
        tFix.teardownOnce(baseDir);
        return 1;
    }
    
    std::cout << "[Test] Server is ready, starting tests...\n";

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    // Clean up files, data, etc.
    tFix.teardownOnce(baseDir);

    if (serverThread.joinable())
        serverThread.join();

    return result;
}
