#include <benchmark/benchmark.h>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "mantisbase/mantis.h"
#include "bench_fixture.h"

namespace {

inline bool ensureServerOrSkip(benchmark::State &state) {
    auto &server = BenchFixture::BenchServer::instance();
    if (!server.isReady() && !server.ensureStarted()) {
        state.SkipWithError("Bench server failed to start");
        return false;
    }
    return true;
}

/// A drogon client pinned to its own EventLoopThread with orderly teardown.
///
/// The HttpClient must be destroyed ON its loop (via runInLoop + wait),
/// mirroring TestHttp::Client::shutdown(). Destroying it on the benchmark
/// thread while an SSE stream is still open is what used to end the run
/// with "forbidden to run loop on threads other than event-loop thread".
/// Only after the client is gone may the loop quit (via ~EventLoopThread).
struct SseProbe {
    explicit SseProbe(int port) {
        loop.run();
        client = drogon::HttpClient::newHttpClient(
            "http://127.0.0.1:" + std::to_string(port), loop.getLoop());
    }

    SseProbe(const SseProbe &) = delete;
    SseProbe &operator=(const SseProbe &) = delete;

    ~SseProbe() {
        shutdown();
    }

    void shutdown() {
        if (!client) {
            return;
        }
        if (auto *l = loop.getLoop()) {
            std::promise<void> done;
            auto doneFuture = done.get_future();
            l->runInLoop([c = std::move(client), &done]() mutable {
                c.reset();
                done.set_value();
            });
            doneFuture.wait();
        } else {
            client.reset();
        }
    }

    trantor::EventLoopThread loop;
    drogon::HttpClientPtr client;
};

} // namespace

static void BM_SSEConnectionSetup(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;
    const int port = BenchFixture::BenchServer::instance().port();

    for (auto _ : state) {
        SseProbe probe(port);

        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/api/v1/realtime?topics=bench_items");

        auto promise = std::make_shared<std::promise<int>>();
        auto future = promise->get_future();

        probe.client->sendRequest(req, [promise](drogon::ReqResult result,
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
        // probe teardown: client reset on its loop, then loop quits.
    }
}
BENCHMARK(BM_SSEConnectionSetup)->Iterations(20);

static void BM_SSEConcurrentConnections(benchmark::State& state) {
    if (!ensureServerOrSkip(state)) return;
    const int port = BenchFixture::BenchServer::instance().port();

    const int num_connections = state.range(0);

    for (auto _ : state) {
        // Probes (loop + client) stay alive together until every request
        // has resolved; previously the client died each loop iteration
        // while its request was still in flight.
        std::vector<std::unique_ptr<SseProbe>> probes;
        std::vector<std::future<int>> futures;
        std::atomic<int> connected{0};

        probes.reserve(num_connections);
        futures.reserve(num_connections);

        for (int i = 0; i < num_connections; ++i) {
            auto probe = std::make_unique<SseProbe>(port);

            auto req = drogon::HttpRequest::newHttpRequest();
            req->setMethod(drogon::Get);
            req->setPath("/api/v1/realtime?topics=bench_items");

            auto promise = std::make_shared<std::promise<int>>();
            futures.push_back(promise->get_future());

            probe->client->sendRequest(req, [promise, &connected](drogon::ReqResult result,
                                                                  const drogon::HttpResponsePtr& resp) {
                if (result == drogon::ReqResult::Ok && resp) {
                    connected.fetch_add(1);
                    promise->set_value(static_cast<int>(resp->statusCode()));
                } else {
                    promise->set_value(0);
                }
            }, 10.0);

            probes.push_back(std::move(probe));
        }

        for (auto& f : futures) {
            auto status = f.wait_for(std::chrono::seconds(10));
            if (status == std::future_status::ready) {
                benchmark::DoNotOptimize(f.get());
            }
        }

        state.counters["connected"] = connected.load();
        // probes destroyed here: each client reset on its own loop first.
    }
}
BENCHMARK(BM_SSEConcurrentConnections)->Arg(5)->Arg(10)->Arg(25);
