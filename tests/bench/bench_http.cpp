#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <filesystem>
#include <memory>

#include "mantisbase/mantis.h"
#include "../common/test_config.h"
#include "../common/test_fixture.h"
#include "../common/test_http_client.h"

namespace fs = std::filesystem;

static std::atomic<bool> server_ready{false};
static std::thread server_thread;
static int bench_port = 0;
static fs::path bench_dir;
static std::unique_ptr<mb::MantisBase> bench_app;

static void startServer() {
    if (server_ready.load()) return;

    TestFixture::setTestEnvVars();
    bench_port = TestFixture::allocateEphemeralPort();
    if (bench_port <= 0) {
        bench_port = 7090;
    }

    bench_dir = TestFixture::makeTestBaseDir("bench");
    bench_app = mb::MantisBase::create(TestFixture::buildAppConfig(bench_dir, bench_port));

    server_thread = std::thread([]() {
        bench_app->run();
    });

    TestHttp::Client cli("127.0.0.1", bench_port);
    for (int i = 0; i < 30; ++i) {
        try {
            if (auto res = cli.Get("/api/v1/health"); res && res->status == 200) {
                server_ready.store(true);
                return;
            }
        } catch (...) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

static std::string getAdminToken() {
    TestHttp::Client cli("127.0.0.1", bench_port);

    nlohmann::json admin_data = {
        {"email", "benchadmin@test.com"},
        {"password", "bench_password_123"}
    };

    try {
        auto admin_entity = bench_app->entity("mb_admins");
        auto existing = admin_entity.queryFromCols("benchadmin@test.com", {"id", "email"});
        if (!existing.has_value()) {
            admin_entity.create(admin_data);
        }
    } catch (...) {}

    nlohmann::json login = {
        {"identity", "benchadmin@test.com"},
        {"password", "bench_password_123"}
    };

    auto res = cli.Post("/api/v1/sys/admins/login", login.dump(), "application/json");
    if (res && res->status == 200) {
        auto body = nlohmann::json::parse(res->body);
        if (body.contains("data") && body["data"].contains("token")) {
            return body["data"]["token"].get<std::string>();
        }
    }
    return "";
}

static void ensureBenchEntity(const std::string& token) {
    TestHttp::Client cli("127.0.0.1", bench_port);
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

    cli.Post("/api/v1/schemas", headers, schema.dump(), "application/json");
}

static void BM_HealthCheck(benchmark::State& state) {
    startServer();
    TestHttp::Client cli("127.0.0.1", bench_port);

    for (auto _ : state) {
        auto res = cli.Get("/api/v1/health");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_HealthCheck)->Iterations(100);

static void BM_GetEntityList(benchmark::State& state) {
    startServer();
    auto token = getAdminToken();
    ensureBenchEntity(token);

    TestHttp::Client cli("127.0.0.1", bench_port);

    for (auto _ : state) {
        auto res = cli.Get("/api/v1/entities/bench_items");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_GetEntityList)->Iterations(100);

static void BM_PostCreateRecord(benchmark::State& state) {
    startServer();
    auto token = getAdminToken();
    ensureBenchEntity(token);

    TestHttp::Client cli("127.0.0.1", bench_port);
    int counter = 0;

    for (auto _ : state) {
        nlohmann::json record = {
            {"title", "bench_item_" + std::to_string(counter++)},
            {"value", counter}
        };
        auto res = cli.Post("/api/v1/entities/bench_items",
                            record.dump(), "application/json");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_PostCreateRecord)->Iterations(100);

static void BM_ConcurrentGetRequests(benchmark::State& state) {
    startServer();
    auto token = getAdminToken();
    ensureBenchEntity(token);

    for (auto _ : state) {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        const int num_clients = state.range(0);

        for (int i = 0; i < num_clients; ++i) {
            threads.emplace_back([&success_count]() {
                TestHttp::Client cli("127.0.0.1", bench_port);
                auto res = cli.Get("/api/v1/entities/bench_items");
                if (res && res->status == 200) {
                    success_count.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        state.counters["success_rate"] = static_cast<double>(success_count.load()) / num_clients;
    }
}
BENCHMARK(BM_ConcurrentGetRequests)->Arg(10)->Arg(50)->Arg(100);
