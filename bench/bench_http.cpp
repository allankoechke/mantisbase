#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <vector>

#include "mantisbase/mantis.h"
#include "bench_fixture.h"
#include "bench_http_client.h"

namespace {

inline int benchPort() {
    return BenchFixture::BenchServer::instance().port();
}

inline bool ensureServerOrSkip(benchmark::State &state) {
    auto &server = BenchFixture::BenchServer::instance();
    if (!server.isReady() && !server.ensureStarted()) {
        state.SkipWithError("Bench server failed to start");
        return false;
    }
    return true;
}

} // namespace

static void BM_HealthCheck(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;
    TestHttp::Client cli("127.0.0.1", benchPort());

    for (auto _ : state) {
        auto res = cli.Get("/api/v1/health");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_HealthCheck)->Iterations(100);

static void BM_GetEntityList(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;
    auto &server = BenchFixture::BenchServer::instance();
    const auto token = BenchFixture::ensureBenchAdminToken(server.app(), server.port());
    BenchFixture::ensureBenchEntity(server.port(), token);

    TestHttp::Client cli("127.0.0.1", benchPort());

    for (auto _ : state) {
        auto res = cli.Get("/api/v1/entities/bench_items");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_GetEntityList)->Iterations(100);

static void BM_PostCreateRecord(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;
    auto &server = BenchFixture::BenchServer::instance();
    const auto token = BenchFixture::ensureBenchAdminToken(server.app(), server.port());
    BenchFixture::ensureBenchEntity(server.port(), token);

    TestHttp::Client cli("127.0.0.1", benchPort());
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
    if (!ensureServerOrSkip(state)) return;
    auto &server = BenchFixture::BenchServer::instance();
    const auto token = BenchFixture::ensureBenchAdminToken(server.app(), server.port());
    BenchFixture::ensureBenchEntity(server.port(), token);

    for (auto _ : state) {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        const auto num_clients = state.range(0);
        const int port = benchPort();

        for (int i = 0; i < num_clients; ++i) {
            threads.emplace_back([&success_count, port]() {
                TestHttp::Client cli("127.0.0.1", port);
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
