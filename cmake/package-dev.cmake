# package-dev.cmake — assemble the mantisbase developer package.
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
#   -DMB_EXPECT_PLATFORMS="linux;windows"  (default: both; platforms whose
#     prebuilt libs must be present)
#
# Inputs:
# - <lib staging>/lib/<os>/static|shared/<arch>/libmantisbase.*  (untouched
#   flow from build-matrix.yml; this script never alters library shipping)
# - <linux/windows headers>: the per-platform trees produced in each build by
#   `cmake --install <build> --component mb-dev-headers`
#   (see cmake/coalesce-headers.cmake). Each is a FULL include tree for its
#   OS — no shared/overlay split, so platform-generated headers
#   (soci-config.h, wolfssl options.h) can never leak across platforms.
#
# Output layout:
#   <out>/include-linux/            full Linux header tree
#   <out>/include-windows/          full Windows header tree
#   <out>/lib/                      prebuilt libraries (copied from staging)
#   <out>/lib/cmake/MantisBase/     MantisBaseConfig.cmake + version file
#   <out>/VERSION  <out>/README.md
#
# Design notes:
# - Only headers reachable from the public API (include/mantisbase/**) are
#   coalesced at build time. jwt-cpp, bcrypt-cpp, zlib and mbs are compiled
#   into the shipped libraries and never #included by public headers.
# - libpq-fe.h / uuid.h are system headers (libpq-dev, uuid-dev); they are
#   documented in README-dev-libs.md, not bundled.

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
if(NOT DEFINED MB_EXPECT_PLATFORMS OR "${MB_EXPECT_PLATFORMS}" STREQUAL "")
    set(MB_EXPECT_PLATFORMS "linux;windows")
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

message(STATUS "MantisBase dev package ${MB_VERSION_TAG} -> ${_OUT}")

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

# --- 1. Per-platform header trees -----------------------------------------------
# Each tree is complete for its OS; assert the load-bearing files so a
# half-empty tree fails loudly instead of shipping a broken package.
set(_TREE_HINT "Run `cmake --install <build> --component mb-dev-headers` in that "
    "platform's build first; see cmake/coalesce-headers.cmake.")

foreach(_os linux windows)
    string(TOUPPER "${_os}" _OS)
    set(_tree "${MB_${_OS}_HEADERS_DIR}")
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
    file(COPY "${_tree}/" DESTINATION "${_OUT}/include-${_os}")
    message(STATUS "  headers [${_os}]: ${_tree} -> ${_OUT}/include-${_os}")
endforeach()

# --- 2. Prebuilt libraries (shipped exactly as the matrix produced them) --------
_require_dir("${_STAGING}/lib" "The *-lib artifacts from build-matrix.yml must be downloaded first.")
file(MAKE_DIRECTORY "${_OUT}/lib")
file(COPY "${_STAGING}/lib/" DESTINATION "${_OUT}/lib")
foreach(_os ${MB_EXPECT_PLATFORMS})
    if(NOT IS_DIRECTORY "${_OUT}/lib/${_os}")
        message(FATAL_ERROR "No prebuilt libraries for platform '${_os}' under ${_OUT}/lib. "
            "Expected lib/${_os}/static|shared/<arch>/ from the build matrix.")
    endif()
    file(GLOB _libs RELATIVE "${_OUT}" "${_OUT}/lib/${_os}/*/*/*")
    if("${_libs}" STREQUAL "")
        message(FATAL_ERROR "No library files found under ${_OUT}/lib/${_os}/ — empty platform dir?")
    endif()
    message(STATUS "  libs [${_os}]: ${_libs}")
endforeach()

# --- 3. CMake package config -----------------------------------------------------
file(MAKE_DIRECTORY "${_OUT}/lib/cmake/MantisBase")
configure_file("${_SRC}/cmake/MantisBaseConfig.cmake.in"
    "${_OUT}/lib/cmake/MantisBase/MantisBaseConfig.cmake" @ONLY)
configure_file("${_SRC}/cmake/MantisBaseConfigVersion.cmake.in"
    "${_OUT}/lib/cmake/MantisBase/MantisBaseConfigVersion.cmake" @ONLY)

# --- 4. Metadata ------------------------------------------------------------------
file(WRITE "${_OUT}/VERSION" "${MB_VERSION_TAG}\n")
_require_file("${_SRC}/.github/release/README-dev-libs.md"
    "README template for the dev package is missing from the repo.")
file(COPY "${_SRC}/.github/release/README-dev-libs.md" DESTINATION "${_OUT}")
file(RENAME "${_OUT}/README-dev-libs.md" "${_OUT}/README.md")

message(STATUS "Dev package assembled:")
message(STATUS "  version : ${MB_VERSION} (${MB_VERSION_TAG})")
message(STATUS "  headers : ${_OUT}/include-linux + ${_OUT}/include-windows")
message(STATUS "  config  : ${_OUT}/lib/cmake/MantisBase/MantisBaseConfig.cmake")
