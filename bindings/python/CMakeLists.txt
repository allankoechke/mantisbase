cmake_minimum_required(VERSION 3.22)
project(mantisbase-python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
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

if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # nanobind compiles Release with -Os, which enables -fdeclone-ctor-dtor.
    # GCC 16 then references a unified std::string constructor that MinGW's
    # libstdc++-6.dll does not export (GCC PR 125359). LoadLibrary reports
    # that as "The specified procedure could not be found."
    # The flag must follow -Os; repeating it after nanobind_opt_size does that.
    target_compile_options(mantisbase_python PRIVATE -fno-declone-ctor-dtor)
    if(TARGET nanobind-static)
        target_compile_options(nanobind-static PRIVATE -fno-declone-ctor-dtor)
    endif()
    # libmantisbase.dll is linked with --export-all-symbols, so its import
    # library also advertises std:: instantiations. Resolving those from the
    # engine DLL fails at load time. Bind libstdc++ first.
    target_link_libraries(mantisbase_python PRIVATE stdc++)
endif()

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
