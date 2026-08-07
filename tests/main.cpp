#include <gtest/gtest.h>
#include <wolfssl/wolfcrypt/wc_port.h>
#include "common/test_fixture.h"

int main(int argc, char *argv[]) {
    const int wolfcrypt_result = wolfCrypt_Init();
    if (wolfcrypt_result != 0) {
        std::cerr << "wolfCrypt_Init failed: "
                  << wolfcrypt_result << '\n';
        return 1;
    }

    TestFixture::setTestEnvVars();
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new MbTestProcessGuard());
    const int test_result = RUN_ALL_TESTS();

    const int cleanup_result = wolfCrypt_Cleanup();
    if (cleanup_result != 0) {
        std::cerr << "wolfCrypt_Cleanup failed: "
                  << cleanup_result << '\n';

        if (test_result == 0) {
            return 1;
        }
    }

    return test_result;
}
