#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>
#include <thread>
#include <atomic>
#include <vector>
#include <future>

#include "mantisbase/mantis.h"
#include "../common/test_http_client.h"

extern std::atomic<bool> server_ready;
extern int bench_port;

static void BM_SSEConnectionSetup(benchmark::State& state) {
    if (!server_ready.load()) {
        state.SkipWithError("Server not ready");
        return;
    }

    for (auto _ : state) {
        trantor::EventLoopThread loopThread;
        loopThread.run();

        auto client = drogon::HttpClient::newHttpClient(
            "http://127.0.0.1:" + std::to_string(bench_port), loopThread.getLoop());

        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/api/v1/realtime?topics=bench_items");

        auto promise = std::make_shared<std::promise<int>>();
        auto future = promise->get_future();

        client->sendRequest(req, [promise](drogon::ReqResult result,
                                           const drogon::HttpResponsePtr& resp) {
            if (result == drogon::ReqResult::Ok && resp) {
                promise->set_value(static_cast<int>(resp->statusCode()));
            } else {
                promise->set_value(0);
            }
        }, 5.0);

        auto status = future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::ready) {
            benchmark::DoNotOptimize(future.get());
        }
    }
}
BENCHMARK(BM_SSEConnectionSetup)->Iterations(20);

static void BM_SSEConcurrentConnections(benchmark::State& state) {
    if (!server_ready.load()) {
        state.SkipWithError("Server not ready");
        return;
    }

    const int num_connections = state.range(0);

    for (auto _ : state) {
        std::vector<std::unique_ptr<trantor::EventLoopThread>> loops;
        std::vector<std::future<int>> futures;
        std::atomic<int> connected{0};

        for (int i = 0; i < num_connections; ++i) {
            auto loop = std::make_unique<trantor::EventLoopThread>();
            loop->run();

            auto client = drogon::HttpClient::newHttpClient(
                "http://127.0.0.1:" + std::to_string(bench_port), loop->getLoop());

            auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);
            req->setPath("/api/v1/realtime?topics=bench_items");

            auto promise = std::make_shared<std::promise<int>>();
            futures.push_back(promise->get_future());

            client->sendRequest(req, [promise, &connected](drogon::ReqResult result,
                                                            const drogon::HttpResponsePtr& resp) {
                if (result == drogon::ReqResult::Ok && resp) {
                    connected.fetch_add(1);
                    promise->set_value(static_cast<int>(resp->statusCode()));
                } else {
                    promise->set_value(0);
                }
            }, 10.0);

            loops.push_back(std::move(loop));
        }

        for (auto& f : futures) {
            auto status = f.wait_for(std::chrono::seconds(10));
            if (status == std::future_status::ready) {
                benchmark::DoNotOptimize(f.get());
            }
        }

        state.counters["connected"] = connected.load();
    }
}
BENCHMARK(BM_SSEConcurrentConnections)->Arg(5)->Arg(10)->Arg(25);
