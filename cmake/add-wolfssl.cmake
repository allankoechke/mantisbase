# Use wolfSSL library in JWT signature signing/verification
set(JWT_SSL_LIBRARY wolfSSL)

# Enable OpenSSL compatibility layer
set(WOLFSSL_OPENSSLEXTRA ON CACHE BOOL "" FORCE)

# Disable tests and examples
set(WOLFSSL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WOLFSSL_CRYPT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Add wolfSSL subdirectory (bundled as git submodule)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/wolfssl)

# Configure mantisbase target with bundled wolfSSL
target_include_directories(mantisbase PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/wolfssl
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/wolfssl/wolfssl
)

target_compile_definitions(mantisbase PUBLIC EXTERNAL_OPTS_OPENVPN)
target_link_libraries(mantisbase PUBLIC wolfssl)