#include <gtest/gtest.h>
#include "common/test_fixture.h"

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new MbTestProcessGuard());
    return RUN_ALL_TESTS();
}
