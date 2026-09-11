#ifndef MANTISBASE_BENCH_FIXTURE_H
#define MANTISBASE_BENCH_FIXTURE_H

// Standalone lifecycle for the benchmark binary.
//
// Intentionally independent of tests/common/test_fixture.h (gtest,
// SharedTestApp, MbTestProcessGuard): mantisbase_bench links neither
// gtest nor benchmark_main — see bench_main.cpp, which owns main() and
// shuts this down deterministically after the benchmarks run.
//
// The two crashes this exists to prevent:
//  1. Exiting with drogon still running and server_thread joinable
//     (static ~std::thread -> std::terminate, preceded by trantor's
//     "forbidden to run loop on threads other than event-loop thread").
//  2. Tearing down drogon HttpClients off their EventLoop while SSE
//     streams are still open (see SseProbe in bench_sse.cpp).

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
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

#include <nlohmann/json.hpp>

#include "mantisbase/mantis.h"
#include "bench_http_client.h"

namespace fs = std::filesystem;

namespace BenchFixture {

inline void setBenchEnvVars() {
#ifdef _WIN32
    _putenv_s("MB_DISABLE_RATE_LIMIT", "1");
    _putenv_s("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1");
    if (std::getenv("MB_JWT_SECRET") == nullptr) {
        _putenv_s("MB_JWT_SECRET", "mantisbase-bench-jwt-secret-do-not-use-in-production");
    }
#else
    setenv("MB_DISABLE_RATE_LIMIT", "1", 1);
    setenv("MB_DISABLE_ADMIN_ON_FIRST_BOOT", "1", 1);
    if (std::getenv("MB_JWT_SECRET") == nullptr) {
        setenv("MB_JWT_SECRET", "mantisbase-bench-jwt-secret-do-not-use-in-production", 1);
    }
#endif
}

// BENCH_PORT wins, then TEST_PORT (CI compat), else an ephemeral port.
inline int allocateBenchPort() {
    const auto configured = mb::getEnvOrDefault("BENCH_PORT", mb::getEnvOrDefault("TEST_PORT", ""));
    if (!configured.empty()) {
        try {
            return std::stoi(configured);
        } catch (...) {
        }
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

inline fs::path makeBenchBaseDir(const std::string &prefix) {
    const auto base_path =
        fs::temp_directory_path() / "mantisbase_bench" / (prefix + "_" + mb::generateShortId());
    fs::create_directories(base_path / "data");
    fs::create_directories(base_path / "www");
    return base_path;
}

inline mb::json buildBenchAppConfig(const fs::path &baseDir, int port) {
    mb::json args;
    args["database"] = "SQLITE";
    args["dataDir"] = (baseDir / "data").string();
    args["publicDir"] = (baseDir / "www").string();
    args["scriptsDir"] = (fs::path(MANTISBASE_TESTS_DIR) / "scripting").string();
    args["serve"] = {{"port", port}, {"host", "127.0.0.1"}, {"skip-admin-setup", true}};
    return args;
}

inline void removeBenchDir(const fs::path &baseDir) {
    try {
        if (!baseDir.empty() && fs::exists(baseDir)) {
            fs::remove_all(baseDir);
        }
    } catch (const std::exception &) {
    }
}

/// Owns the bench server end-to-end: app + server thread + temp dir.
/// Idempotent start/stop; destructor stops (so server_thread is never
/// joinable at exit). Drogon runs one HTTP lifecycle per process, and
/// this binary is its own process, so a singleton is the right shape.
class BenchServer {
public:
    static BenchServer &instance() {
        static BenchServer inst;
        return inst;
    }

    BenchServer(const BenchServer &) = delete;
    BenchServer &operator=(const BenchServer &) = delete;

    ~BenchServer() {
        shutdown();
    }

    /// Start the server once; poll /health until ready. True when ready.
    bool ensureStarted() {
        {
            std::lock_guard lock(mutex_);
            if (started_ && ready_.load()) {
                return true;
            }
            if (started_) {
                return ready_.load();
            }

            setBenchEnvVars();
            port_ = allocateBenchPort();
            if (port_ <= 0) {
                port_ = 7090;
            }

            baseDir_ = makeBenchBaseDir("bench");
            app_ = mb::MantisBase::create(buildBenchAppConfig(baseDir_, port_));

            serverRunning_.store(true);
            serverThread_ = std::thread([this]() {
                try {
                    [[maybe_unused]] auto rc = app_->run();
                } catch (const std::exception &) {
                }
                serverRunning_.store(false);
            });
            started_ = true;
        }

        TestHttp::Client cli("127.0.0.1", port_);
        for (int i = 0; i < 30; ++i) {
            try {
                if (const auto res = cli.Get("/api/v1/health"); res && res->status == 200) {
                    ready_.store(true);
                    return true;
                }
            } catch (...) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        shutdown();
        return false;
    }

    /// Stop drogon, join the server thread, close units, drop temp dir.
    /// Safe to call repeatedly and from the destructor.
    void shutdown() {
        std::unique_lock lock(mutex_);
        if (!started_ && !app_) {
            return;
        }

        // 1. Ask drogon's loops to quit (thread-safe; queued to the loops).
        try {
            if (app_ && app_->router().isRunning()) {
                app_->router().close();
            }
        } catch (const std::exception &) {
        }

        // 2. Join the thread blocked in app().run() without holding the lock.
        std::thread t = std::move(serverThread_);
        lock.unlock();
        if (t.joinable()) {
            t.join();
        }
        lock.lock();

        // 3. Close units and release; MantisBase::~close() after this is a no-op.
        try {
            if (app_) {
                app_->close();
                app_.reset();
            }
        } catch (const std::exception &) {
        }

        removeBenchDir(baseDir_);
        baseDir_.clear();
        started_ = false;
        ready_.store(false);
        serverRunning_.store(false);
    }

    [[nodiscard]] bool isReady() const {
        return ready_.load();
    }

    [[nodiscard]] int port() const {
        return port_;
    }

    mb::MantisBase &app() {
        return *app_;
    }

private:
    BenchServer() = default;

    mutable std::mutex mutex_;
    std::unique_ptr<mb::MantisBase> app_;
    fs::path baseDir_;
    int port_{0};
    bool started_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> serverRunning_{false};
    std::thread serverThread_;
};

/// Create-or-find the bench admin and return a login token ("" on failure).
inline std::string ensureBenchAdminToken(mb::MantisBase &app, int port) {
    TestHttp::Client cli("127.0.0.1", port);

    nlohmann::json admin_data = {
        {"email", "benchadmin@test.com"},
        {"password", "bench_password_123"}
    };

    try {
        auto admin_entity = app.entity("mb_admins");
        auto existing = admin_entity.queryFromCols("benchadmin@test.com", {"id", "email"});
        if (!existing.has_value()) {
            (void)admin_entity.create(admin_data);
        }
    } catch (...) {
    }

    nlohmann::json login = {
        {"identity", "benchadmin@test.com"},
        {"password", "bench_password_123"}
    };

    try {
        auto res = cli.Post("/api/v1/sys/admins/login", login.dump(), "application/json");
        if (res && res->status == 200) {
            auto body = nlohmann::json::parse(res->body);
            if (body.contains("data") && body["data"].contains("token")) {
                return body["data"]["token"].get<std::string>();
            }
        }
    } catch (...) {
    }
    return "";
}

/// Idempotent: POST the bench_items schema (201 on create, 409 if present).
inline void ensureBenchEntity(int port, const std::string &token) {
    TestHttp::Client cli("127.0.0.1", port);
    TestHttp::Headers headers = {{"Authorization", "Bearer " + token}};

    nlohmann::json schema = {
        {"name", "bench_items"},
        {"type", "base"},
        {
            "rules", {
                {"list", {{"mode", "public"}}},
                {"get", {{"mode", "public"}}},
                {"add", {{"mode", "public"}}},
                {"update", {{"mode", "public"}}},
                {"delete", {{"mode", "public"}}}
            }
        },
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}, {"required", true}},
            {{"name", "value"}, {"type", "int"}}
        })}
    };

    try {
        cli.Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    } catch (...) {
    }
}

} // namespace BenchFixture

#endif // MANTISBASE_BENCH_FIXTURE_H
