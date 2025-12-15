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

file(GLOB TEST_FILES
        main.cpp
        unit/test_database.cpp
        unit/test_models.cpp
        unit/test_expr_eval.cpp
        unit/test_jwt.cpp
        unit/test_access_rules.cpp
        unit/test_context_store.cpp
        unit/test_entity_schema_field.cpp
        unit/test_files_security.cpp
        integration/test_integration_crud.cpp
        integration/test_integration_schema.cpp
        integration/test_integration_auth.cpp
)

add_executable(mantisbase_tests ${TEST_FILES})

if (NOT WIN32) # For now, only enable ASAN for non-windows based builds
    message("Enabling Address Sanitizer")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/asan.cmake)
    include(../cmake/asan.cmake)
    set(ENABLE_ASAN ON)

    enable_asan_for_target(mantisbase_tests)
endif ()

target_compile_definitions(mantisbase_tests PRIVATE
        TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(mantisbase_tests PRIVATE
            -Wa,-mbig-obj  # Tell assembler to handle large objects
    )
endif ()

target_link_libraries(mantisbase_tests PRIVATE
        mantisbase_lib
        GTest::gtest
        GTest::gtest_main
        GTest::gmock_main
)

# Register tests with CTest
include(GoogleTest)
add_test(NAME all_mantisbase_tests COMMAND mantisbase_tests)