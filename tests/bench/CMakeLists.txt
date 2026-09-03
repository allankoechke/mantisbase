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

# Try to find GTest first
find_package(GTest QUIET)

if (NOT GTest_FOUND)
    # If not found, fetch it using FetchContent
    include(FetchContent)

    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0  # Use latest stable version
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Don't build gmock/gtest apps
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)
endif ()

add_executable(mantisbase_bench
    bench_http.cpp
    bench_sse.cpp
    bench_crud.cpp
)

include(GoogleTest)

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
    GTest::gtest
    GTest::gtest_main
    GTest::gmock_main
)
