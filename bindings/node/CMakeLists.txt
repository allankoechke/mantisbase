cmake_minimum_required(VERSION 3.22)
project(mantisbase-node LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# When driven by cmake-js, CMAKE_JS_INC is set automatically.
# When included from the parent CMakeLists.txt without cmake-js, skip gracefully.
if(NOT CMAKE_JS_INC)
    message(STATUS "[mantisbase-node] cmake-js variables not found; skipping Node.js addon.")
    message(STATUS "  Build via: cd bindings/node && npm install")
    return()
endif()

# Locate node-addon-api headers
execute_process(
    COMMAND node -p "require('node-addon-api').include"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _napi_result
)
if(NOT _napi_result EQUAL 0)
    message(FATAL_ERROR "Failed to locate node-addon-api. Run 'npm install' in ${CMAKE_CURRENT_SOURCE_DIR} first.")
endif()
string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

# Addon shared library
add_library(mantisbase_node SHARED
    src/addon.cpp
    src/app_wrap.cpp
    src/router_wrap.cpp
    src/request_wrap.cpp
    src/response_wrap.cpp
    src/db_wrap.cpp
)

target_include_directories(mantisbase_node PRIVATE
    ${CMAKE_JS_INC}
    ${NODE_ADDON_API_DIR}
)

# Link against mantisbase shared library
if(TARGET mb::shared)
    target_link_libraries(mantisbase_node PRIVATE mb::shared ${CMAKE_JS_LIB})
else()
    find_library(MANTISBASE_LIB mantisbase REQUIRED)
    find_path(MANTISBASE_INCLUDE mantisbase/mantisbase.h REQUIRED)
    target_link_libraries(mantisbase_node PRIVATE ${MANTISBASE_LIB} ${CMAKE_JS_LIB})
    target_include_directories(mantisbase_node PRIVATE ${MANTISBASE_INCLUDE})
endif()

set_target_properties(mantisbase_node PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    OUTPUT_NAME "mantisbase"
)

target_compile_definitions(mantisbase_node PRIVATE
    NAPI_VERSION=8
    NAPI_DISABLE_CPP_EXCEPTIONS
)
