# package-dev.cmake — assemble the mantisbase developer package.
#
# Run from the release workflow (or locally) as:
#   cmake -P cmake/package-dev.cmake \
#     -DMB_SOURCE_DIR=<repo root> \
#     -DMB_VERSION_TAG=v0.4.3 \
#     -DMB_LIB_STAGING_DIR=<merged per-platform lib artifacts> \
#     -DMB_OUTPUT_DIR=<destination dev-package root>
#
# Optional:
#   -DMB_VERSION=0.4.3              (default: MB_VERSION_TAG with leading 'v' stripped)
#   -DMB_EXPECT_PLATFORMS="linux;windows"  (default: both; platforms whose
#     prebuilt libs + generated headers must be present)
#
# Expected staging layout (produced by build-matrix.yml):
#   <staging>/lib/<os>/static|shared/<arch>/libmantisbase.*
#   <staging>/generated-include/mantisbase/config.hpp
#   <staging>/generated/<os>/drogon/exports.h
#   <staging>/generated/<os>/trantor/exports.h
#   <staging>/generated/<os>/soci/soci-config.h
#   <staging>/generated/<os>/wolfssl/options.h
#
# Output layout:
#   <out>/include/                  mantisbase headers + portable 3rd-party headers
#   <out>/include-<os>/             per-platform generated headers (soci-config.h, options.h)
#   <out>/lib/                      prebuilt libraries (copied from staging)
#   <out>/lib/cmake/MantisBase/     MantisBaseConfig.cmake + version file
#   <out>/VERSION  <out>/README.md
#
# Design notes:
# - Only headers reachable from the public API (include/mantisbase/**) are
#   bundled. jwt-cpp, bcrypt-cpp, zlib and mbs are compiled into the shipped
#   libraries and never #included by public headers, so they are excluded.
# - libpq-fe.h / uuid.h are system headers (libpq-dev, uuid-dev); they are
#   documented in README-dev-libs.md, not bundled.
# - The per-platform overlays (include-<os>/) exist because soci-config.h and
#   wolfssl/options.h are generated from build options and differ per OS.
#   drogon/exports.h and trantor/exports.h are verified portable, so they live
#   in the shared include/ tree.

cmake_minimum_required(VERSION 3.22)

foreach(_var MB_SOURCE_DIR MB_VERSION_TAG MB_LIB_STAGING_DIR MB_OUTPUT_DIR)
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

# Copy a header tree, keeping only headers (some source dirs also hold .c/.txt).
macro(_copy_headers src_dir dst_dir)
    _require_dir("${src_dir}" "Check the 3rdParty submodule list — was it checked out recursively?")
    file(COPY "${src_dir}/" DESTINATION "${dst_dir}"
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endmacro()

file(MAKE_DIRECTORY "${_OUT}/include" "${_OUT}/lib")

# --- 1. MantisBase public headers + generated config.hpp ----------------------
_copy_headers("${_SRC}/include/mantisbase" "${_OUT}/include/mantisbase")
file(REMOVE "${_OUT}/include/mantisbase/config.hpp.in")

_require_file("${_STAGING}/generated-include/mantisbase/config.hpp"
    "The build-matrix job for x86_64 must upload generated-include/mantisbase/config.hpp.")
file(COPY "${_STAGING}/generated-include/mantisbase/config.hpp"
    DESTINATION "${_OUT}/include/mantisbase")

# --- 2. Portable 3rd-party header trees ---------------------------------------
_copy_headers("${_SRC}/3rdParty/argparse/include/argparse" "${_OUT}/include/argparse")
_copy_headers("${_SRC}/3rdParty/dukglue/include/dukglue" "${_OUT}/include/dukglue")
_copy_headers("${_SRC}/3rdParty/drogon/lib/inc/drogon" "${_OUT}/include/drogon")
_copy_headers("${_SRC}/3rdParty/drogon/trantor/trantor" "${_OUT}/include/trantor")
_copy_headers("${_SRC}/3rdParty/json/single_include/nlohmann" "${_OUT}/include/nlohmann")
_copy_headers("${_SRC}/3rdParty/jsoncpp/include/json" "${_OUT}/include/json")
_copy_headers("${_SRC}/3rdParty/soci/include/soci" "${_OUT}/include/soci")
_copy_headers("${_SRC}/3rdParty/spdlog/include/spdlog" "${_OUT}/include/spdlog")
_copy_headers("${_SRC}/3rdParty/fmt/include/fmt" "${_OUT}/include/fmt")
_copy_headers("${_SRC}/3rdParty/wolfssl/wolfssl" "${_OUT}/include/wolfssl")

# duktape ships its headers next to sources: pick the two headers, not duktape.c.
foreach(_duk_header duktape.h duk_config.h)
    _require_file("${_SRC}/3rdParty/duktape/${_duk_header}"
        "Check the 3rdParty/duktape submodule.")
    file(COPY "${_SRC}/3rdParty/duktape/${_duk_header}" DESTINATION "${_OUT}/include")
endforeach()

# soci-config.h is generated (never ship a stale in-tree copy if one appears).
file(REMOVE "${_OUT}/include/soci/soci-config.h")

# --- 3. Generated headers ------------------------------------------------------
# drogon/exports.h and trantor/exports.h are portable across the matrix, so the
# Linux copy seeds the shared tree; the per-OS overlays below still take
# precedence for soci-config.h / wolfssl options.h, which differ per platform.
set(_GEN_HINT "The build-matrix lib-staging upload for that platform is missing "
    "its generated headers. See the 'Stage generated headers' step in build-matrix.yml.")

_require_file("${_STAGING}/generated/linux/drogon/exports.h" "${_GEN_HINT}")
_require_file("${_STAGING}/generated/linux/trantor/exports.h" "${_GEN_HINT}")
file(COPY "${_STAGING}/generated/linux/drogon/exports.h" DESTINATION "${_OUT}/include/drogon")
file(COPY "${_STAGING}/generated/linux/trantor/exports.h" DESTINATION "${_OUT}/include/trantor")

foreach(_os ${MB_EXPECT_PLATFORMS})
    file(MAKE_DIRECTORY "${_OUT}/include-${_os}")
    foreach(_gen soci/soci-config.h wolfssl/options.h drogon/exports.h trantor/exports.h)
        _require_file("${_STAGING}/generated/${_os}/${_gen}" "${_GEN_HINT}")
        get_filename_component(_gen_dir "${_gen}" DIRECTORY)
        file(MAKE_DIRECTORY "${_OUT}/include-${_os}/${_gen_dir}")
        file(COPY "${_STAGING}/generated/${_os}/${_gen}"
            DESTINATION "${_OUT}/include-${_os}/${_gen_dir}")
    endforeach()
endforeach()

# --- 4. Prebuilt libraries ------------------------------------------------------
_require_dir("${_STAGING}/lib" "The *-lib artifacts from build-matrix.yml must be downloaded first.")
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

# --- 5. CMake package config -----------------------------------------------------
file(MAKE_DIRECTORY "${_OUT}/lib/cmake/MantisBase")
configure_file("${_SRC}/cmake/MantisBaseConfig.cmake.in"
    "${_OUT}/lib/cmake/MantisBase/MantisBaseConfig.cmake" @ONLY)
configure_file("${_SRC}/cmake/MantisBaseConfigVersion.cmake.in"
    "${_OUT}/lib/cmake/MantisBase/MantisBaseConfigVersion.cmake" @ONLY)

# --- 6. Metadata ------------------------------------------------------------------
file(WRITE "${_OUT}/VERSION" "${MB_VERSION_TAG}\n")
_require_file("${_SRC}/.github/release/README-dev-libs.md"
    "README template for the dev package is missing from the repo.")
file(COPY "${_SRC}/.github/release/README-dev-libs.md" DESTINATION "${_OUT}")
file(RENAME "${_OUT}/README-dev-libs.md" "${_OUT}/README.md")

message(STATUS "Dev package assembled:")
message(STATUS "  version : ${MB_VERSION} (${MB_VERSION_TAG})")
message(STATUS "  headers : ${_OUT}/include (+ ${_OUT}/include-<os> overlays)")
message(STATUS "  config  : ${_OUT}/lib/cmake/MantisBase/MantisBaseConfig.cmake")
