# package-dev.cmake — assemble the per-OS mantisbase developer packages.
#
# Run from the release workflow (or locally) with -D options BEFORE -P:
#   cmake \
#     -DMB_SOURCE_DIR=<repo root> \
#     -DMB_VERSION_TAG=v0.4.3 \
#     -DMB_LIB_STAGING_DIR=<merged per-platform lib artifacts> \
#     -DMB_LINUX_HEADERS_DIR=<linux coalesced include tree> \
#     -DMB_WINDOWS_HEADERS_DIR=<windows coalesced include tree> \
#     -DMB_OUTPUT_DIR=<destination dev-package root> \
#     -P cmake/package-dev.cmake
#
# Optional:
#   -DMB_VERSION=0.4.3              (default: MB_VERSION_TAG with leading 'v' stripped)
#   -DMB_PLATFORMS="linux;windows"  (default: both; platforms to assemble)
#
# Inputs:
# - <lib staging>/<os>/libs/<arch>/libmantisbase.so
#   (<os>/libs/<arch>/libmantisbase.dll + libmantisbase.dll.a on Windows)
#   where <arch> is x86 or arm (see pkg_arch in build-matrix.yml).
#   Only shared libraries are shipped — no static archives.
# - <linux/windows headers>: the per-platform trees produced in each build by
#   `cmake --install <build> --component mb-dev-headers`
#   (see cmake/coalesce-headers.cmake). Each is a FULL include tree for its
#   OS — no shared/overlay split, so platform-generated headers
#   (soci-config.h, wolfssl options.h) can never leak across platforms.
#
# Output layout (one directory per OS, zipped separately by release.yml):
#   <out>/<os>/CMakeLists.txt         add_subdirectory() entry point
#   <out>/<os>/README.md              quick start (from cmake/dev-package/)
#   <out>/<os>/VERSION                release tag
#   <out>/<os>/include/               full header tree for this OS
#   <out>/<os>/libs/<arch>/           prebuilt shared library
#   <out>/<os>/lib/cmake/MantisBase/  MantisBaseConfig.cmake + version file
#
# Design notes:
# - Only headers reachable from the public API (include/mantisbase/**) are
#   coalesced at build time. jwt-cpp, bcrypt-cpp, zlib and mbs are compiled
#   into the shipped libraries and never #included by public headers.
# - libpq-fe.h / uuid.h are system headers (libpq-dev, uuid-dev); they are
#   documented in the package README, not bundled.

cmake_minimum_required(VERSION 3.22)

foreach(_var MB_SOURCE_DIR MB_VERSION_TAG MB_LIB_STAGING_DIR
        MB_LINUX_HEADERS_DIR MB_WINDOWS_HEADERS_DIR MB_OUTPUT_DIR)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR "package-dev.cmake requires -D${_var}=<...>")
    endif()
endforeach()

if(NOT DEFINED MB_VERSION OR "${MB_VERSION}" STREQUAL "")
    string(REGEX REPLACE "^v" "" MB_VERSION "${MB_VERSION_TAG}")
endif()
if(NOT DEFINED MB_PLATFORMS OR "${MB_PLATFORMS}" STREQUAL "")
    set(MB_PLATFORMS "linux;windows")
endif()

# Numeric components for MantisBaseConfigVersion.cmake.in. A trailing
# prerelease suffix (e.g. 0.4.3-rc.1) is tolerated: only the leading
# major.minor.patch triple is extracted.
if(NOT MB_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    message(FATAL_ERROR "MB_VERSION '${MB_VERSION}' must start with major.minor.patch "
        "(e.g. 0.4.3, optionally followed by -rc.1).")
endif()
set(MB_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(MB_VERSION_MINOR "${CMAKE_MATCH_2}")
set(MB_VERSION_PATCH "${CMAKE_MATCH_3}")

set(_SRC "${MB_SOURCE_DIR}")
set(_STAGING "${MB_LIB_STAGING_DIR}")
set(_OUT "${MB_OUTPUT_DIR}")

# Normalize to absolute paths (anchored at the invoking working directory).
# This matters because file(GLOB) resolves relative patterns against the
# script's own directory, while COPY/WRITE/EXISTS resolve against the
# invoking cwd — absolute paths behave identically for every command.
foreach(_var _SRC _STAGING _OUT MB_LINUX_HEADERS_DIR MB_WINDOWS_HEADERS_DIR)
    get_filename_component(${_var} "${${_var}}" ABSOLUTE)
endforeach()

message(STATUS "MantisBase dev packages ${MB_VERSION_TAG} -> ${_OUT}")

# --- Helpers -----------------------------------------------------------------
macro(_require_file path hint)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing required file: ${path}\n${hint}")
    endif()
endmacro()

macro(_require_dir path hint)
    if(NOT IS_DIRECTORY "${path}")
        message(FATAL_ERROR "Missing required directory: ${path}\n${hint}")
    endif()
endmacro()

# --- Per-OS packages ------------------------------------------------------------
set(_TREE_HINT "Run `cmake --install <build> --component mb-dev-headers` in that "
    "platform's build first; see cmake/coalesce-headers.cmake.")
set(_LIB_HINT "The *-lib artifacts from build-matrix.yml must stage "
    "<os>/libs/<arch>/ shared libraries first (shared only, no static).")

foreach(_os ${MB_PLATFORMS})
    string(TOUPPER "${_os}" _OS)
    set(_tree "${MB_${_OS}_HEADERS_DIR}")
    set(_pkg "${_OUT}/${_os}")

    # 1. Headers (full tree for this OS).
    _require_dir("${_tree}" "${_TREE_HINT}")
    foreach(_probe
            mantisbase/mantisbase.h mantisbase/config.hpp
            drogon/HttpRequest.h drogon/exports.h
            drogon/orm/DbClient.h trantor/exports.h
            nlohmann/json.hpp json/json.h
            soci/soci.h soci/soci-config.h
            spdlog/spdlog.h fmt/format.h
            dukglue/dukglue.h duktape.h
            wolfssl/wolfio.h wolfssl/options.h)
        _require_file("${_tree}/${_probe}" "${_TREE_HINT}")
    endforeach()
    file(COPY "${_tree}/" DESTINATION "${_pkg}/include")
    message(STATUS "  [${_os}] headers: ${_tree} -> ${_pkg}/include")

    # 2. Shared libraries (one folder per architecture).
    _require_dir("${_STAGING}/${_os}/libs" "${_LIB_HINT}")
    file(COPY "${_STAGING}/${_os}/libs/" DESTINATION "${_pkg}/libs")
    file(GLOB _arch_dirs RELATIVE "${_pkg}/libs" "${_pkg}/libs/*")
    if("${_arch_dirs}" STREQUAL "")
        message(FATAL_ERROR "No architecture folders under ${_pkg}/libs/ — empty libs dir?")
    endif()
    foreach(_arch IN LISTS _arch_dirs)
        if(_os STREQUAL "windows")
            _require_file("${_pkg}/libs/${_arch}/libmantisbase.dll" "${_LIB_HINT}")
            _require_file("${_pkg}/libs/${_arch}/libmantisbase.dll.a" "${_LIB_HINT}")
        else()
            _require_file("${_pkg}/libs/${_arch}/libmantisbase.so" "${_LIB_HINT}")
        endif()
    endforeach()
    message(STATUS "  [${_os}] libs: arches ${_arch_dirs}")

    # 3. CMake integration entry point + quick readme + version.
    if(_os STREQUAL "windows")
        set(MB_OS "windows")
        set(MB_OS_PRETTY "Windows")
        set(MB_LIB_FILE "libmantisbase.dll (+ import lib libmantisbase.dll.a)")
        set(MB_PREREQS "No system packages required. On MinGW the socket/RPC/IP-helper/crypto system libraries (`ws2_32`, `rpcrt4`, `iphlpapi`, `crypt32`) are linked automatically.")
        set(MB_MANUAL "Add `include/` to the header search path and link the matching import library plus its system dependencies:\n\n```bat\n:: Example: Windows x86-64 (MinGW), shared\nrem Ensure libmantisbase.dll is next to your .exe at runtime\n```\n\n```cmake\n# CMakeLists.txt (manual, without the bundled entry point)\ntarget_include_directories(my_app PRIVATE path/to/include)\ntarget_link_libraries(my_app PRIVATE path/to/libs/x86/libmantisbase.dll.a ws2_32 rpcrt4 iphlpapi crypt32)\n```")
    else()
        set(MB_OS "linux")
        set(MB_OS_PRETTY "Linux")
        set(MB_LIB_FILE "libmantisbase.so")
        set(MB_PREREQS "The shared library needs its system dependencies at build and run time. Install them with:\n\n```bash\nsudo apt-get update\nsudo apt-get install -y libpq-dev uuid-dev\n```\n\nRuntime shared libraries on Debian/Ubuntu are `libpq5` and `libuuid1` (see `docker/Dockerfile` in the source repo).")
        set(MB_MANUAL "Add `include/` to the header search path and link the matching shared library plus its system dependencies (make sure the `.so` is findable at runtime, e.g. via `rpath` or `LD_LIBRARY_PATH`):\n\n```bash\n# Example: Linux x86-64, shared\ng++ -std=c++20 main.cpp -I path/to/include -L path/to/libs/x86 -lmantisbase -Wl,-rpath,path/to/libs/x86 -lpq -luuid -ldl -lpthread -lm -o my_app\n```")
    endif()
    configure_file("${_SRC}/cmake/dev-package/README.md.in"
        "${_pkg}/README.md" @ONLY)
    file(WRITE "${_pkg}/VERSION" "${MB_VERSION_TAG}\n")

    # 4. CMake package config (find_package alternative).
    file(MAKE_DIRECTORY "${_pkg}/cmake")
    configure_file("${_SRC}/cmake/dev-package/MantisBaseConfig.cmake.in"
        "${_pkg}/cmake/MantisBaseConfig.cmake" @ONLY)
    configure_file("${_SRC}/cmake/dev-package/MantisBaseConfigVersion.cmake.in"
        "${_pkg}/cmake/MantisBaseConfigVersion.cmake" @ONLY)

    message(STATUS "  [${_os}] package: ${_pkg}")
endforeach()

message(STATUS "Dev packages assembled:")
message(STATUS "  version : ${MB_VERSION} (${MB_VERSION_TAG})")
message(STATUS "  packages: ${_OUT}/linux + ${_OUT}/windows")
