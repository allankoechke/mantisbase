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

static void BM_SequentialInserts(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;

    TestHttp::Client cli("127.0.0.1", benchPort());
    int counter = 0;

    for (auto _ : state) {
        nlohmann::json record = {
            {"title", "crud_bench_" + std::to_string(counter++)},
            {"value", counter}
        };
        auto res = cli.Post("/api/v1/entities/bench_items",
                            record.dump(), "application/json");
        benchmark::DoNotOptimize(res);
    }

    state.counters["inserts_per_sec"] = benchmark::Counter(
        counter, benchmark::Counter::kIsRate);
}
BENCHMARK(BM_SequentialInserts)->Iterations(200);

static void BM_SequentialReads(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;

    TestHttp::Client cli("127.0.0.1", benchPort());

    for (auto _ : state) {
        auto res = cli.Get("/api/v1/entities/bench_items?limit=50");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_SequentialReads)->Iterations(200);

static void BM_ConcurrentInserts(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;

    const auto concurrency = state.range(0);
    const int port = benchPort();

    for (auto _ : state) {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        std::atomic<int> counter{0};

        for (int i = 0; i < concurrency; ++i) {
            threads.emplace_back([&success_count, &counter, port]() {
                TestHttp::Client cli("127.0.0.1", port);
                int id = counter.fetch_add(1);
                const nlohmann::json record = {
                    {"title", "concurrent_" + std::to_string(id)},
                    {"value", id}
                };
                const auto res = cli.Post("/api/v1/entities/bench_items",
                                    record.dump(), "application/json");
                if (res && res->status == 201) {
                    success_count.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        state.counters["success_count"] = success_count.load();
        state.counters["success_rate"] = static_cast<double>(success_count.load()) / concurrency;
    }
}
BENCHMARK(BM_ConcurrentInserts)->Arg(5)->Arg(10)->Arg(25)->Arg(50);

static void BM_ConcurrentReads(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;

    const int concurrency = state.range(0);
    const int port = benchPort();

    for (auto _ : state) {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i = 0; i < concurrency; ++i) {
            threads.emplace_back([&success_count, port]() {
                TestHttp::Client cli("127.0.0.1", port);
                auto res = cli.Get("/api/v1/entities/bench_items?limit=10");
                if (res && res->status == 200) {
                    success_count.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        state.counters["success_count"] = success_count.load();
        state.counters["success_rate"] = static_cast<double>(success_count.load()) / concurrency;
    }
}
BENCHMARK(BM_ConcurrentReads)->Arg(5)->Arg(10)->Arg(25)->Arg(50);

static void BM_MixedCRUD(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;

    const int concurrency = state.range(0);
    const int port = benchPort();

    for (auto _ : state) {
        std::vector<std::thread> threads;
        std::atomic<int> ops{0};

        for (int i = 0; i < concurrency; ++i) {
            threads.emplace_back([i, &ops, port]() {
                TestHttp::Client cli("127.0.0.1", port);

                if (i % 3 == 0) {
                    const nlohmann::json record = {
                        {"title", "mixed_" + std::to_string(i)},
                        {"value", i}
                    };
                    cli.Post("/api/v1/entities/bench_items",
                             record.dump(), "application/json");
                } else {
                    cli.Get("/api/v1/entities/bench_items?limit=10");
                }
                ops.fetch_add(1);
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        state.counters["total_ops"] = ops.load();
    }
}
BENCHMARK(BM_MixedCRUD)->Arg(10)->Arg(25)->Arg(50);
