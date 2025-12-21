#include <gtest/gtest.h>
#include "common/test_environment.h"

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    // Set up MantisBase environment for all tests (unit and integration)
    // The environment will handle server startup/shutdown automatically
    const auto mb_env = std::make_unique<MantisBaseTestEnvironment>();
    ::testing::AddGlobalTestEnvironment(mb_env.get());
    
    // Run all tests - environment SetUp() runs before tests, TearDown() runs after
    return RUN_ALL_TESTS();
}

