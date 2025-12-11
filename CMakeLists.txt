cmake_minimum_required(VERSION 3.22)
project(MantisBase VERSION 0.3.0)
set(CMAKE_CXX_STANDARD 20)

# Set output directories to match SOCI's
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
option(MANTIS_SCRIPTING_ENABLED "Enable JS Scripting" OFF)

# Flag for building deps as shared/static libs
set(MANTIS_SHARED_DEPS FALSE)
if(MANTIS_SHARED_DEPS)
    set(BUILD_SHARED_LIBS TRUE)
    set(MANTIS_LIB_TYPE SHARED)
    message("-- Building Mantis dynamic libs")
else()
    set(BUILD_SHARED_LIBS FALSE)
    set(MANTIS_LIB_TYPE STATIC)

    # For static builds, embed libstdgcc and libstdc++ to avoid binary loading fails
    # due to mismatch libstd* libs.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    message("-- Building Mantis static libs")
endif()

set( MANTIS_SOURCES
        src/mantisbase.cpp
        src/core/database.cpp
        src/core/logger.cpp
        src/core/router.cpp
        src/core/route_registry.cpp
        src/core/auth.cpp

        # Expression evaluator
        src/core/expr_evaluator.cpp

        # Utilities
        src/utils/auth_utils.cpp
        src/utils/string_utils.cpp
        src/utils/path_utils.cpp
        src/utils/date_utils.cpp

        include/mantisbase/core/private-impl/soci_custom_types.hpp
        include/mantisbase/config.hpp.in

        src/core/kv_store.cpp
        src/core/files.cpp
        src/core/context_store.cpp

        src/core/http_request.cpp
        src/core/http_response.cpp

        src/core/models/entity.cpp
        src/parse_cmd.cpp

        src/core/models/validators.cpp
        src/core/middlewares.cpp

        src/core/models/entity_schema.cpp
        src/core/models/entity_schema_base_fields.cpp
        src/core/models/entity_schema_crud.cpp
        src/core/models/entity_schema_field.cpp
        src/core/models/entity_routes_handlers.cpp
        src/core/models/entity_schema_routes_handlers.cpp

        src/core/exceptions.cpp
        src/core/router_httplib_internals.cpp
        src/core/models/access_rules.cpp

        src/utils/dukglue_utils_bindings.cpp
        src/core/http_content_reader.cpp
        # src/core/private-impl/duktape_wrapper.cpp

        src/core/exceptions.cpp
)

# Add library
add_library(mantisbase_lib ${MANTIS_LIB_TYPE} ${MANTIS_SOURCES})

# Add main executable
add_executable(mantisbase_app src/main.cpp)

# Ensure resultant name is always `mantisbase` despite targets names for consistency
set_target_properties(mantisbase_lib PROPERTIES OUTPUT_NAME mantisbase)
set_target_properties(mantisbase_app PROPERTIES OUTPUT_NAME mantisbase)

if(MANTIS_SCRIPTING_ENABLED)
    message("-- Enabling Mantis Scripting via Duktape")
    target_compile_definitions(mantis PRIVATE MANTIS_SCRIPTING_ENABLED=1)
endif (MANTIS_SCRIPTING_ENABLED)

# Add httplib
include(cmake/add-httplib.cmake)

# Add spdlog
if(MANTIS_SHARED_DEPS)
    # Build Shared lib for spdlog
    set(SPDLOG_BUILD_SHARED ON)
else()
    # Build static lib for spdlog
    set(SPDLOG_BUILD_SHARED OFF)
endif()

# Add SPDLOG subdir
add_subdirectory(3rdParty/spdlog)

# Add JSON
include(cmake/add-json.cmake)

# Add SOCI
include(cmake/add-soci.cmake)

# Add wolfssl library
include(cmake/add-wolfssl.cmake)

# Add bcrypt to the project
include(cmake/add-bcrypt.cmake)

# Add admin assets using cmrc
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/cmrc)
add_subdirectory(3rdParty/mb-admin)

# Include address sanitizer for Linux debug/releaseWithDebug builds
option(MANTIS_BUILD_WITH_ASAN "Enable ASAN build for UNIX" ON)
include(cmake/add-asan.cmake)

# Include duktape for ECMASCRIPT scripting in Mantis
include(cmake/add-duktape.cmake)

# Include directories
target_include_directories(mantisbase_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/httplib-cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/argparse/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/jwt-cpp/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(mantisbase_app
    PRIVATE mantisbase_lib
)

# Check if Doxygen is installed, then build docs
include(cmake/add-doxygen.cmake)

# Add SEMVER
include(cmake/add-semver.cmake)

# Add Tests
option(MANTIS_BUILD_TESTS "Build Mantis Tests" ON)
include(cmake/add-tests.cmake)

configure_file(
        include/mantisbase/config.hpp.in
        ${CMAKE_CURRENT_SOURCE_DIR}/include/mantisbase/config.hpp
)

# Link to libs
target_link_libraries(mantisbase_lib
        PUBLIC
        httplib::httplib
        spdlog::spdlog
)