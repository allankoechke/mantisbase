# MantisBase prebuilt developer package.
#
# Integrate with add_subdirectory():
#
#   add_subdirectory(path/to/mantisbase-linux-cpp-dev)
#   target_link_libraries(your_app PRIVATE mantisbase)
#
# This file picks the shared library matching your architecture out of
# libs/<arch>/ and exposes it as the imported target `mantisbase`, with the
# bundled headers and required system libraries attached.
#
# Layout of this package:
#   CMakeLists.txt   this file (add_subdirectory entry point)
#   README.md        quick start
#   VERSION          release tag
#   include/         header tree for this OS
#   libs/<arch>/     prebuilt shared library (x86, arm)
#   lib/cmake/MantisBase/  find_package(MantisBase) config (alternative)

cmake_minimum_required(VERSION 3.22)

# Guard against double inclusion.
if(TARGET mantisbase)
    return()
endif()

# --- Headers ---------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/include/mantisbase/mantisbase.h")
    message(FATAL_ERROR "MantisBase headers not found under "
        "'${CMAKE_CURRENT_LIST_DIR}/include'. Is the dev package extracted intact?")
endif()

# --- Architecture ------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(_MB_PKG_ARCH "x86")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|ARM64|arm64)$")
    set(_MB_PKG_ARCH "arm")
else()
    message(FATAL_ERROR "MantisBase dev package does not support architecture "
        "'${CMAKE_SYSTEM_PROCESSOR}'. Supported: x86-64 (libs/x86), aarch64 (libs/arm).")
endif()

# --- Shared library ------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_MB_PKG_LIB "${CMAKE_CURRENT_LIST_DIR}/libs/${_MB_PKG_ARCH}/libmantisbase.so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_MB_PKG_LIB "${CMAKE_CURRENT_LIST_DIR}/libs/${_MB_PKG_ARCH}/libmantisbase.dll")
    set(_MB_PKG_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/libs/${_MB_PKG_ARCH}/libmantisbase.dll.a")
else()
    message(FATAL_ERROR "MantisBase dev package does not support '${CMAKE_SYSTEM_NAME}'. "
        "Download the dev package matching your OS.")
endif()

if(NOT EXISTS "${_MB_PKG_LIB}")
    message(FATAL_ERROR "MantisBase shared library not found: '${_MB_PKG_LIB}'. "
        "Make sure the dev package matches your OS and CPU architecture.")
endif()

# --- System dependencies -------------------------------------------------------
find_package(Threads REQUIRED)
set(_MB_PKG_SYSTEM_LIBS Threads::Threads)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # PostgreSQL client (soci_postgresql) and uuid (drogon) come from the
    # system package manager:  sudo apt-get install -y libpq-dev uuid-dev
    find_library(_MB_PKG_PQ_LIB pq)
    if(NOT _MB_PKG_PQ_LIB)
        message(FATAL_ERROR "MantisBase requires libpq (PostgreSQL client). "
            "Install it with: sudo apt-get install -y libpq-dev")
    endif()
    find_library(_MB_PKG_UUID_LIB uuid)
    if(NOT _MB_PKG_UUID_LIB)
        message(FATAL_ERROR "MantisBase requires libuuid. "
            "Install it with: sudo apt-get install -y uuid-dev")
    endif()
    list(APPEND _MB_PKG_SYSTEM_LIBS ${_MB_PKG_PQ_LIB} ${_MB_PKG_UUID_LIB} ${CMAKE_DL_LIBS} m)
else()
    # MinGW system libraries used by MantisBase (sockets, RPC/UUID, IP helper, crypto).
    list(APPEND _MB_PKG_SYSTEM_LIBS ws2_32 rpcrt4 iphlpapi crypt32)
endif()

# --- Compile definitions ---------------------------------------------------------
# EXTERNAL_OPTS_OPENVPN makes wolfssl/wolfcrypt/settings.h pull in the
# matching wolfssl/options.h from this package's include/ tree instead of
# falling back to unconfigured defaults (ABI match with the prebuilt lib).
set(_MB_PKG_DEFS EXTERNAL_OPTS_OPENVPN)

# --- Imported target --------------------------------------------------------------
add_library(mantisbase SHARED IMPORTED)
set_target_properties(mantisbase PROPERTIES
    IMPORTED_LOCATION "${_MB_PKG_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    INTERFACE_LINK_LIBRARIES "${_MB_PKG_SYSTEM_LIBS}"
    INTERFACE_COMPILE_DEFINITIONS "${_MB_PKG_DEFS}"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND EXISTS "${_MB_PKG_IMPLIB}")
    set_target_properties(mantisbase PROPERTIES
        IMPORTED_IMPLIB "${_MB_PKG_IMPLIB}"
    )
endif()

unset(_MB_PKG_ARCH)
unset(_MB_PKG_LIB)
unset(_MB_PKG_IMPLIB)
unset(_MB_PKG_SYSTEM_LIBS)
unset(_MB_PKG_DEFS)
unset(_MB_PKG_PQ_LIB)
unset(_MB_PKG_UUID_LIB)
