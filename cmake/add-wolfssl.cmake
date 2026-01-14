# Use wolfSSL library in JWT signature signing/verification
# in place of OpenSSL
set(JWT_SSL_LIBRARY wolfSSL)

# Enable OpenSSL compatibility layer
set(WOLFSSL_OPENSSLEXTRA ON CACHE BOOL "" FORCE)

# Disable tests and examples
set(WOLFSSL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WOLFSSL_CRYPT_TESTS OFF CACHE BOOL "" FORCE)

# Force static library build
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# GitHub CI fails with string overflow warning treated as an error
# so, lets disable it.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overflow")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/wolfssl)

target_include_directories(mantisbase  PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/wolfssl
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/wolfssl/wolfssl
)

target_link_libraries(mantisbase PRIVATE wolfssl)