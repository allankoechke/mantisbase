#ifndef MANTISBASE_TEST_FIXTURE_H
#define MANTISBASE_TEST_FIXTURE_H

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "../include/mantisbase/mantis.h"
#include "test_config.h"
#include "test_helpers.h"
#include "test_http_client.h"

namespace fs = std::filesystem;

namespace TestFixture {

inline void setTestEnvVars() {
#ifdef _WIN32
    _putenv_s("MB_DISABLE_RATE_LIMIT", "1");
    _putenv_s("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1");
#else
    setenv("MB_DISABLE_RATE_LIMIT", "1", 1);
    setenv("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1", 1);
#endif
}

inline int allocateEphemeralPort() {
    const auto env_port = mb::getEnvOrDefault("TEST_PORT", "");
    if (!env_port.empty()) {
        return std::stoi(env_port);
    }

#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 0;
    }
#endif

    const int sock = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (sock < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    int port = 0;
    if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
        socklen_t len = sizeof(addr);
        if (getsockname(sock, reinterpret_cast<sockaddr *>(&addr), &len) == 0) {
            port = ntohs(addr.sin_port);
        }
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    return port;
}

inline fs::path makeTestBaseDir(const std::string &prefix) {
    const auto base_path = fs::temp_directory_path() / "mantisbase_tests" /
                           (prefix + "_" + mb::generateShortId());
    fs::create_directories(base_path / "data");
    fs::create_directories(base_path / "www");
    return base_path;
}

inline mb::json buildAppConfig(const fs::path &baseDir, const std::optional<int> &port = std::nullopt) {
    mb::json args;
    args["dev"] = true;
    args["database"] = "SQLITE";
    args["dataDir"] = (baseDir / "data").string();
    args["publicDir"] = (baseDir / "www").string();
    args["scriptsDir"] = (fs::path(TEST_SOURCE_DIR) / "scripting").string();
    if (port.has_value()) {
        args["serve"] = {{"port", *port}, {"host", "127.0.0.1"}};
    }
    return args;
}

inline void removeTestDir(const fs::path &baseDir) {
    try {
        if (!baseDir.empty() && fs::exists(baseDir)) {
            fs::remove_all(baseDir);
        }
    } catch (const std::exception &) {
    }
}

/// Drogon only supports one HTTP lifecycle per process. All fixtures share a
/// single MantisBase instance; HTTP starts on first MbServerFixture use.
class SharedTestApp {
public:
    static SharedTestApp &instance() {
        static SharedTestApp inst;
        return inst;
    }

    mb::MantisBase &acquire() {
        std::lock_guard lock(mutex_);
        if (!app_) {
            TestFixture::setTestEnvVars();
            baseDir_ = makeTestBaseDir("shared");
            port_ = allocateEphemeralPort();
            if (port_ <= 0) {
                throw std::runtime_error("Failed to allocate ephemeral port");
            }
            app_ = mb::MantisBase::create(buildAppConfig(baseDir_, port_));
        }
        ++refCount_;
        return *app_;
    }

    void release() {
        std::lock_guard lock(mutex_);
        if (refCount_ > 0) {
            --refCount_;
        }
    }

    mb::MantisBase &ensureServerRunning() {
        auto &app = acquire();
        std::lock_guard lock(mutex_);
        if (!serverStarted_) {
            serverThread_ = std::thread([this]() {
                serverRunning_.store(true);
                try {
                    app_->run();
                } catch (const std::exception &) {
                }
                serverRunning_.store(false);
            });

            TestHttp::Client cli("127.0.0.1", port_);
            if (!TestHelpers::waitForServer(cli, 30, 200)) {
                shutdownServerLocked();
                throw std::runtime_error("Server failed to start on port " + std::to_string(port_));
            }
            serverStarted_ = true;
        }
        return app;
    }

    [[nodiscard]] int port() const { return port_; }

    static void shutdownAll() {
        auto &self = instance();
        std::lock_guard lock(self.mutex_);
        self.shutdownServerLocked();
        if (self.app_) {
            self.app_->close();
            self.app_.reset();
        }
        removeTestDir(self.baseDir_);
        self.refCount_ = 0;
        self.serverStarted_ = false;
    }

private:
    void shutdownServerLocked() {
        if (!app_ || !serverStarted_) {
            return;
        }
        try {
            if (app_->router().isRunning()) {
                app_->router().close();
            }
        } catch (const std::exception &) {
        }

        if (serverThread_.joinable()) {
            const auto start = std::chrono::steady_clock::now();
            while (serverRunning_.load() &&
                   std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
        }
        serverStarted_ = false;
    }

    std::mutex mutex_;
    std::unique_ptr<mb::MantisBase> app_;
    fs::path baseDir_;
    int port_{0};
    int refCount_{0};
    bool serverStarted_{false};
    std::thread serverThread_;
    std::atomic<bool> serverRunning_{false};
};

} // namespace TestFixture

class MbAppFixture : public ::testing::Test {
protected:
    void SetUp() override {
        app_ = &TestFixture::SharedTestApp::instance().acquire();
    }

    void TearDown() override {
        TestFixture::SharedTestApp::instance().release();
    }

    [[nodiscard]] mb::MantisBase &mantis() { return *app_; }

private:
    mb::MantisBase *app_{nullptr};
};

class MbServerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        app_ = &TestFixture::SharedTestApp::instance().ensureServerRunning();
        port_ = TestFixture::SharedTestApp::instance().port();
    }

    void TearDown() override {
        TestFixture::SharedTestApp::instance().release();
    }

    [[nodiscard]] mb::MantisBase &mantis() { return *app_; }

    [[nodiscard]] TestHttp::Client httpClient() const {
        return TestHttp::Client("127.0.0.1", port_);
    }

    [[nodiscard]] int getPort() const { return port_; }

private:
    mb::MantisBase *app_{nullptr};
    int port_{0};
};

class MbTestProcessGuard : public ::testing::Environment {
public:
    void TearDown() override {
        TestFixture::SharedTestApp::shutdownAll();
    }
};

#endif // MANTISBASE_TEST_FIXTURE_H
