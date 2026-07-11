add_definitions(-D_WIN32_WINNT=0x0A00)

# Build static lib for httplib
set(HTTPLIB_COMPILE OFF CACHE BOOL "")
if(WIN32)
    set(HTTPLIB_USE_NON_BLOCKING_GETADDRINFO OFF)
else (WIN32)
    include(cmake/FindZSTD.cmake)
    find_package(zstd)

    if(zstd_FOUND)
        set(HTTPLIB_REQUIRE_ZSTD TRUE)
        if(NOT TARGET zstd::libzstd)
            find_package(ZSTD QUIET)
            if(TARGET ZSTD::ZSTD)
                add_library(zstd::libzstd ALIAS ZSTD::ZSTD)
            endif()
        endif()
    else()
        set(HTTPLIB_REQUIRE_ZSTD FALSE)
    endif()
endif()

# Disable SSL use for now, throws errors with WolfSSL library version
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)

add_subdirectory(3rdParty/httplib-cpp)