// Custom entry point for mantisbase_bench.
//
// We deliberately do NOT link benchmark_main: after the benchmarks run we
// must shut the bench server down in a defined order — quit drogon, join
// the server thread, close app units — before any static destructor runs.
// Relying on statics for that ordering is what produced
// "It is forbidden to run loop on threads other than event-loop thread"
// followed by "terminate called without an active exception".

#include <benchmark/benchmark.h>

#include "bench_fixture.h"

int main(int argc, char **argv) {
    ::benchmark::Initialize(&argc, argv);
    const auto result = ::benchmark::RunSpecifiedBenchmarks();
    std::cout << "\n*** " << result << " ***\n";
    ::benchmark::Shutdown();
    std::cout << "\n*** SHUTDOWN COMPLETE ***\n";

    // Deterministic teardown while drogon's globals are still alive.
    // BenchServer's destructor repeats this as a safety net.
    BenchFixture::BenchServer::instance().shutdown();

    std::cout << "\n*** INSTANCE TEARDOWN COMPLETE ***\n";

    // result is the number of benchmarks run, not an exit status:
    // always exit 0 here so shells/CI don't read success as failure.
    return 0;
}
