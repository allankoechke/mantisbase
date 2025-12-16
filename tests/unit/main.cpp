#include <gtest/gtest.h>
#include "../common/test_environment.h"

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Set up MantisBase environment for all unit tests
    ::testing::AddGlobalTestEnvironment(new MantisBaseTestEnvironment);
    
    return RUN_ALL_TESTS();
}

