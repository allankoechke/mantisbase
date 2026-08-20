cmake_minimum_required(VERSION 3.22)
project(mantisbase-python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# When included from the parent CMakeLists.txt, nanobind may not be installed.
# Skip gracefully so the rest of the build can proceed.
find_package(Python 3.9 COMPONENTS Interpreter Development.Module QUIET)
if(NOT Python_FOUND)
    message(STATUS "[mantisbase-python] Python >= 3.9 not found; skipping Python bindings.")
    message(STATUS "  Build via: pip install -e bindings/python")
    return()
endif()

# Locate nanobind via Python's package metadata
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_VARIABLE nanobind_ROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _nb_result
    ERROR_QUIET
)
if(NOT _nb_result EQUAL 0)
    message(STATUS "[mantisbase-python] nanobind not found; skipping Python bindings.")
    message(STATUS "  Install nanobind: pip install nanobind")
    return()
endif()

find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(mantisbase_python
    src/module.cpp
    src/app_bind.cpp
    src/router_bind.cpp
    src/request_bind.cpp
    src/response_bind.cpp
    src/db_bind.cpp
)

# Link against mantisbase shared library
if(TARGET mb::shared)
    target_link_libraries(mantisbase_python PRIVATE mb::shared)
else()
    find_library(MANTISBASE_LIB mantisbase REQUIRED)
    find_path(MANTISBASE_INCLUDE mantisbase/mantisbase.h REQUIRED)
    target_link_libraries(mantisbase_python PRIVATE ${MANTISBASE_LIB})
    target_include_directories(mantisbase_python PRIVATE ${MANTISBASE_INCLUDE})
endif()

set_target_properties(mantisbase_python PROPERTIES
    OUTPUT_NAME "mantisbase"
)

install(TARGETS mantisbase_python LIBRARY DESTINATION .)
