#ifndef MANTISBASE_TEST_ENVIRONMENT_H
#define MANTISBASE_TEST_ENVIRONMENT_H

#include <gtest/gtest.h>
#include <filesystem>
#include <cstdlib>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <cstdlib>
#endif
#include "../test_fixure.h"
#include "test_config.h"

namespace fs = std::filesystem;

/**
 * @brief GoogleTest Environment to set up MantisBase for unit tests
 * 
 * This ensures MantisBase is initialized once before all tests run,
 * similar to how main.cpp does it for integration tests.
 */
class MantisBaseTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Disable rate limiting in tests
        #ifdef _WIN32
            _putenv_s("TEST_DISABLE_RATE_LIMIT", "1");
        #else
            setenv("TEST_DISABLE_RATE_LIMIT", "1", 1);
        #endif
        
        // Setup directories for tests
        baseDir = getBaseDir();
        const auto scriptingDir = (fs::path(TEST_SOURCE_DIR) / "scripting").string();
        const auto dataDir = (baseDir / "data").string();
        const auto publicDir = (baseDir / "www").string();

        mb::json args;
        // args["dev"] = true;
        args["database"] = "SQLITE";
        args["dataDir"] = dataDir;
        args["publicDir"] = publicDir;
        args["scriptsDir"] = scriptingDir;
        args["serve"] = {{"port", 0}, {"host", "0.0.0.0"}}; // Port 0 = don't start server

        // mb::logger::trace("Test Environment Args: {}", args.dump());

        // Setup MantisBase instance (without starting server for unit tests)
        TestFixture::instance(args);
    }

    void TearDown() override {
        // Clean up
        if (!baseDir.empty() && fs::exists(baseDir)) {
            try {
                TestFixture::app().close();
                fs::remove_all(baseDir);
            } catch (const std::exception& e) {
                std::cerr << "[TestEnvironment] Error during cleanup: " << e.what() << "\n";
            }
        }
    }

private:
    fs::path baseDir;
};

#endif //MANTISBASE_TEST_ENVIRONMENT_H

