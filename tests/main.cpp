#include <gtest/gtest.h>
#include <iostream>
#include <cstdlib>
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

    mb::json args;
    args["database"] = "SQLITE";
    args["dataDir"] = dataDir;
    args["publicDir"] = publicDir;
    args["scriptsDir"] = scriptingDir;
    args["serve"] = {{"port", TestConfig::getTestPort()}, {"host", "0.0.0.0"}};

    mb::logger::trace("Args: {}", args.dump());

    // Setup Db, Server, etc.
    auto& tFix = TestFixture::instance(args);

    // Spawn a new thread to run the
    auto serverThread = std::thread([&tFix]()
    {
        try
        {
            auto& app = tFix.app();
            [[maybe_unused]] auto res = app.run();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error Starting Server: " << e.what() << '\n';
        }
    });

    // Wait for server to be ready (replaces fixed sleep)
    if (!tFix.waitForServer(20, 100)) {
        std::cerr << "[Test] Server failed to start within timeout\n";
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
