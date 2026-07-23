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

add_executable(mantisbase_bench
    bench_http.cpp
    bench_sse.cpp
    bench_crud.cpp
)

target_compile_definitions(mantisbase_bench PRIVATE
    TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(mantisbase_bench PRIVATE
        -Wa,-mbig-obj
    )
endif ()

target_link_libraries(mantisbase_bench PRIVATE
    mantisbase
    benchmark::benchmark
    benchmark::benchmark_main
)
