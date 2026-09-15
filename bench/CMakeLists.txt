include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(benchmark)

# NOTE: mantisbase_bench intentionally links neither benchmark_main nor
# GTest, and uses nothing under tests/. It owns main() in bench_main.cpp
# so the bench server (drogon has one HTTP lifecycle per process) can be
# shut down deterministically — quit drogon, join the server thread, close
# app units — before static destruction. The only tests/ asset it needs is
# the scripting dir below, located via MANTISBASE_TESTS_DIR.
add_executable(mantisbase_bench
    bench_main.cpp
    bench_http.cpp
    bench_sse.cpp
    bench_crud.cpp
)

target_compile_definitions(mantisbase_bench PRIVATE
    MANTISBASE_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tests"
)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(mantisbase_bench PRIVATE
        -Wa,-mbig-obj
    )
endif ()

target_link_libraries(mantisbase_bench PRIVATE
    mantisbase
    benchmark::benchmark
)
