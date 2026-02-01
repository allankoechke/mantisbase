#include <gtest/gtest.h>
#include "common/test_environment.h"

std::atomic<bool> MbTestEnv::server_ready{false};

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new MbTestEnv());
    return RUN_ALL_TESTS();
}