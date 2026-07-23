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
#include <utility>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <cstdlib>
#endif
#include "../include/mantisbase/mantis.h"
#include "test_config.h"
#include "test_http_client.h"

namespace fs = std::filesystem;

class MbTestEnv : public ::testing::Environment {
public:
    MbTestEnv();

    ~MbTestEnv() override;

    MbTestEnv(const MbTestEnv &) = delete;

    MbTestEnv &operator=(const MbTestEnv &) = delete;

    void SetUp() override;

    void TearDown() override;

    static MbTestEnv &instance();

    [[nodiscard]] bool waitForServer(int max_retries = 20, int initial_delay_ms = 50) const;

    [[nodiscard]] TestHttp::Client client() const;

    static TestHttp::Client staticClient();

    static void cleanDir(const fs::path &base_path);

    [[nodiscard]] int getPort() const;

private:
    static MbTestEnv *&getInstancePtr();

    static fs::path getBaseDir();

    void shutdown();

    fs::path baseDir;
    std::thread serverThread;
    std::atomic<bool> server_running{false};
    int port{0};
    static std::atomic<bool> server_ready;
};

inline MbTestEnv::MbTestEnv() {
    getInstancePtr() = this;

#ifdef _WIN32
    _putenv_s("MB_DISABLE_RATE_LIMIT", "1");
    _putenv_s("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1");
#else
    setenv("MB_DISABLE_RATE_LIMIT", "1", 1);
    setenv("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1", 1);
#endif

    baseDir = getBaseDir();
    const auto scriptingDir = (fs::path(TEST_SOURCE_DIR) / "scripting").string();
    const auto dataDir = (baseDir / "data").string();
    const auto publicDir = (baseDir / "www").string();

    port = TestConfig::getTestPort();

    mb::json args;
    args["dev"] = true;
    args["database"] = "SQLITE";
    args["dataDir"] = dataDir;
    args["publicDir"] = publicDir;
    args["scriptsDir"] = scriptingDir;
    args["serve"] = {{"port", port}, {"host", "0.0.0.0"}};

    mb::MantisBase::create(args);
}

inline MbTestEnv::~MbTestEnv() = default;

inline void MbTestEnv::SetUp() {
    serverThread = std::thread([this]() {
        try {
            std::cout << "[MbTestEnv] Starting server in background thread...\n";
            server_running.store(true);
            auto res = mb::MantisBase::instance().run();
            std::cout << "[MbTestEnv] Server thread: app.run() returned with code " << res << '\n';
        } catch (const std::exception &e) {
            std::cerr << "[MbTestEnv] Server thread error: " << e.what() << '\n';
        }

        server_running.store(false);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[MbTestEnv] Waiting for server to be ready...\n";
    if (!waitForServer(30, 200)) {
        std::cerr << "[MbTestEnv] Server failed to start within timeout\n";
        shutdown();
        return;
    }

    std::cout << "[MbTestEnv] Server is ready, tests can begin...\n";
}

inline void MbTestEnv::TearDown() {
    shutdown();
}

inline MbTestEnv &MbTestEnv::instance() {
    std::cout << "[MbTestEnv] MbTestEnv::instance1() called\n";
    std::cout.flush();

    MbTestEnv *inst = getInstancePtr();
    if (inst == nullptr) {
        std::cerr << "[MbTestEnv] MbTestEnv::instance() called but inst is nullptr\n";
        throw std::runtime_error("MbTestEnv::instance() called before SetUp()");
    }
    return *inst;
}

inline bool MbTestEnv::waitForServer(const int max_retries, const int initial_delay_ms) const {
    if (server_ready.load()) {
        return true;
    }

    int delay_ms = initial_delay_ms;
    for (int i = 0; i < max_retries; ++i) {
        try {
            TestHttp::Client cli("127.0.0.1", port);

            if (auto res = cli.Get("/api/v1/health")) {
                if (res->status == 200) {
                    server_ready.store(true);
                    std::cout << "[MbTestEnv] Health check passed (attempt " << (i + 1) << ")\n";
                    return true;
                }
            }
        } catch (const std::exception &e) {
            if (i % 5 == 0) {
                std::cout << "[MbTestEnv] Health check exception: " << e.what()
                        << " (attempt " << (i + 1) << ")\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        delay_ms = std::min(delay_ms * 2, 500);
    }

    std::cerr << "[MbTestEnv] Health check failed after " << max_retries << " attempts\n";
    return false;
}

inline TestHttp::Client MbTestEnv::client() const {
    return TestHttp::Client("127.0.0.1", port);
}

inline TestHttp::Client MbTestEnv::staticClient() {
    return TestHttp::Client("127.0.0.1", TestConfig::getTestPort());
}

inline void MbTestEnv::cleanDir(const fs::path &base_path) {
    try {
        if (fs::exists(base_path)) {
            fs::remove_all(base_path);
            std::cout << "[MbTestEnv] Removed directory " << base_path.string() << "...\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "[MbTestEnv] Cleaning up failed: " << e.what() << std::endl;
    }
    server_ready.store(false);
}

inline int MbTestEnv::getPort() const {
    return port;
}

inline MbTestEnv *&MbTestEnv::getInstancePtr() {
    std::cout << "[MbTestEnv] MbTestEnv::getInstancePtr() called\n";
    std::cout.flush();
    static MbTestEnv *instance_ptr = nullptr;
    return instance_ptr;
}

inline fs::path MbTestEnv::getBaseDir() {
    auto base_path = fs::temp_directory_path() / "mantisbase_tests" / mb::generateShortId();
    try {
        fs::create_directories(base_path);
        fs::permissions(base_path, fs::perms::owner_all, fs::perm_options::replace);
    } catch (const std::exception &e) {
        std::cerr << "[FS Create] " << e.what() << std::endl;
    }

    return base_path;
}

inline void MbTestEnv::shutdown() {
    std::cout << "[MbTestEnv] Test environment shutting down ...\n";
    try {
        if (auto &router = mb::MantisBase::instance().router();
            router.isRunning()) router.close();
    } catch (const std::exception &e) {
        std::cerr << "[MbTestEnv] Error stopping server: " << e.what() << '\n';
    }

    if (serverThread.joinable()) {
        std::cout << "[MbTestEnv] Waiting for server thread to exit...\n";

        const auto quick_timeout = std::chrono::milliseconds(500);
        const auto start = std::chrono::steady_clock::now();

        while (server_running.load() &&
               (std::chrono::steady_clock::now() - start) < quick_timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (serverThread.joinable()) {
            if (!server_running.load()) {
                serverThread.join();
                std::cout << "[MbTestEnv] Server thread joined successfully.\n";
            } else {
                std::cerr << "[MbTestEnv] WARNING: Server thread still running, detaching to prevent hang...\n";
                serverThread.detach();
            }
        }
    }

    if (!baseDir.empty()) {
        std::cout << "[MbTestEnv] Cleaning up test directory...\n";
        cleanDir(baseDir);
    }

    if (serverThread.joinable()) {
        std::cerr << "[MbTestEnv] WARNING: Thread still joinable, detaching...\n";
        serverThread.detach();
    }
}

#endif //MANTISBASE_TEST_ENVIRONMENT_H
