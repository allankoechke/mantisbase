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

# CPython's Windows import library is an MSVC .lib. MinGW ld records data
# exports (Py_None and the rest) as functions, and loading the extension
# fails with "The specified procedure could not be found." The wheel job
# builds a GNU import library with gendef/dlltool and points here.
if(MINGW AND DEFINED ENV{PYTHON_MINGW_LIB})
    set(_mb_pyimp "$ENV{PYTHON_MINGW_LIB}")
    if(NOT EXISTS "${_mb_pyimp}")
        message(FATAL_ERROR "PYTHON_MINGW_LIB does not exist: ${_mb_pyimp}")
    endif()
    foreach(_mb_cfg "" "_RELEASE" "_DEBUG" "_RELWITHDEBINFO" "_MINSIZEREL")
        set_target_properties(Python::Module PROPERTIES
            "IMPORTED_IMPLIB${_mb_cfg}" "${_mb_pyimp}")
    endforeach()
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
    src/py_conv.cpp
    src/app_bind.cpp
    src/router_bind.cpp
    src/request_bind.cpp
    src/response_bind.cpp
    src/db_bind.cpp
    src/entity_bind.cpp
    src/auth_bind.cpp
    src/services_bind.cpp
    src/middleware_bind.cpp
    src/utils_bind.cpp
)

target_include_directories(mantisbase_python PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # nanobind compiles Release with -Os, which enables -fdeclone-ctor-dtor.
    # GCC 16 then references a unified std::string constructor that MinGW's
    # libstdc++-6.dll does not export (GCC PR 125359). LoadLibrary reports
    # that as "The specified procedure could not be found."
    # The flag must follow -Os; repeating it after nanobind_opt_size does that.
    # The static lib is nanobind-static, or nanobind-static-abi3 / -ft / -fa
    # depending on the Python build. Every one of those objects is linked in.
    target_compile_options(mantisbase_python PRIVATE -fno-declone-ctor-dtor)
    foreach(_mb_nb
            nanobind-static nanobind-static-abi3
            nanobind-static-ft nanobind-static-abi3-ft
            nanobind-static-fa nanobind-static-fa-ft
            nanobind-static-ps nanobind-static-abi3-ps)
        if(TARGET ${_mb_nb})
            target_compile_options(${_mb_nb} PRIVATE -fno-declone-ctor-dtor)
        endif()
    endforeach()
    # libmantisbase.dll is linked with --export-all-symbols, so its import
    # library also advertises std:: instantiations. Resolving those from the
    # engine DLL fails at load time. Bind libstdc++ first.
    target_link_libraries(mantisbase_python PRIVATE stdc++)
    # Same missing libstdc++-6.dll symbols as the engine DLL. The extension
    # is a separate link, so it needs its own copy of the definitions.
    set(_mb_cxx_compat
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/platform/mingw_libstdcxx_compat.cpp)
    set_source_files_properties(${_mb_cxx_compat} PROPERTIES COMPILE_OPTIONS "-fno-weak")
    target_sources(mantisbase_python PRIVATE ${_mb_cxx_compat})
endif()

# Link against mantisbase shared library
if(TARGET mb::shared)
    target_link_libraries(mantisbase_python PRIVATE mb::shared)
else()
    find_library(MANTISBASE_LIB mantisbase)
    find_path(MANTISBASE_INCLUDE mantisbase/mantisbase.h)
    if(NOT MANTISBASE_LIB OR NOT MANTISBASE_INCLUDE)
        message(FATAL_ERROR
            "Could not find the mantisbase shared library and headers.\n"
            "Wheels are published for CPython 3.10-3.14. A Python outside that set makes pip/uv build this sdist, which does not compile the C++ engine.\n"
            "To build from source, install the engine first and set CMAKE_PREFIX_PATH to the prefix that contains lib/libmantisbase.so and include/mantisbase/mantisbase.h.")
    endif()
    target_link_libraries(mantisbase_python PRIVATE ${MANTISBASE_LIB})
    target_include_directories(mantisbase_python PRIVATE ${MANTISBASE_INCLUDE})
endif()

set_target_properties(mantisbase_python PROPERTIES
    OUTPUT_NAME "mantisbase"
)

install(TARGETS mantisbase_python LIBRARY DESTINATION .)
