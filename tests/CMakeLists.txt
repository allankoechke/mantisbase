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

# Option to build combined tests (backward compatibility) or separate
option(BUILD_COMBINED_TESTS "Build all tests in one executable" OFF)

if (BUILD_COMBINED_TESTS)
    # Combined tests (backward compatibility)
    file(GLOB TEST_FILES
            test_fixure.cpp
            main.cpp
            unit/test_database.cpp
            unit/test_models.cpp
            unit/test_expr_eval.cpp
            unit/test_jwt.cpp
            unit/test_access_rules.cpp
            unit/test_context_store.cpp
            unit/test_entity_schema_field.cpp
            unit/test_files_security.cpp
            unit/test_ip_validation.cpp
#            integration/test_integration_crud.cpp
#            integration/test_integration_schema.cpp
#            integration/test_integration_auth.cpp
    )

    add_executable(mantisbase_tests ${TEST_FILES})

    if (NOT WIN32)
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
                -Wa,-mbig-obj
        )
    endif ()

    target_link_libraries(mantisbase_tests PRIVATE
            mantisbase_lib
            GTest::gtest
            GTest::gtest_main
            GTest::gmock_main
    )

    include(GoogleTest)
    add_test(NAME all_mantisbase_tests COMMAND mantisbase_tests)
else ()
    # Separate unit and integration tests (recommended)
    
    # Unit Tests (no server needed)
    file(GLOB UNIT_TEST_FILES
            test_fixure.cpp
            unit/main.cpp
            unit/test_database.cpp
            unit/test_models.cpp
            unit/test_expr_eval.cpp
            unit/test_jwt.cpp
            unit/test_access_rules.cpp
            unit/test_context_store.cpp
            unit/test_entity_schema_field.cpp
            unit/test_files_security.cpp
            unit/test_ip_validation.cpp
    )

    add_executable(mantisbase_unit_tests ${UNIT_TEST_FILES})

    if (NOT WIN32)
        message("Enabling Address Sanitizer for unit tests")
        include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/asan.cmake)
        include(../cmake/asan.cmake)
        set(ENABLE_ASAN ON)
        enable_asan_for_target(mantisbase_unit_tests)
    endif ()

    target_compile_definitions(mantisbase_unit_tests PRIVATE
            TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(mantisbase_unit_tests PRIVATE
                -Wa,-mbig-obj
        )
    endif ()

    target_link_libraries(mantisbase_unit_tests PRIVATE
            mantisbase_lib
            GTest::gtest
            GTest::gmock_main
    )

    include(GoogleTest)
    add_test(NAME unit_tests COMMAND mantisbase_unit_tests)
    
    # Integration Tests (require server)
    file(GLOB INTEGRATION_TEST_FILES
            main.cpp
#            integration/test_integration_crud.cpp
#            integration/test_integration_schema.cpp
#            integration/test_integration_auth.cpp
    )

    add_executable(mantisbase_integration_tests ${INTEGRATION_TEST_FILES})

    if (NOT WIN32)
        message("Enabling Address Sanitizer for integration tests")
        include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/asan.cmake)
        include(../cmake/asan.cmake)
        set(ENABLE_ASAN ON)
        enable_asan_for_target(mantisbase_integration_tests)
    endif ()

    target_compile_definitions(mantisbase_integration_tests PRIVATE
            TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(mantisbase_integration_tests PRIVATE
                -Wa,-mbig-obj
        )
    endif ()

    target_link_libraries(mantisbase_integration_tests PRIVATE
            mantisbase_lib
            GTest::gtest
            GTest::gtest_main
            GTest::gmock_main
    )

    include(GoogleTest)
    add_test(NAME integration_tests COMMAND mantisbase_integration_tests)
endif ()