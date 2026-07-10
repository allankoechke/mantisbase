add_definitions(-D_WIN32_WINNT=0x0A00)

include(FetchContent)

# Disable all non-core Drogon features
set(BUILD_CTL OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_ORM OFF CACHE BOOL "" FORCE)
set(BUILD_BROTLI OFF CACHE BOOL "" FORCE)
set(BUILD_YAML_CONFIG OFF CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Disable TLS in Trantor (wolfSSL handles JWT signing separately)
set(TRANTOR_USE_TLS "none" CACHE STRING "" FORCE)

FetchContent_Declare(
    drogon
    GIT_REPOSITORY https://github.com/drogonframework/drogon.git
    GIT_TAG v1.9.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(drogon)

target_link_libraries(mantisbase PUBLIC Drogon::Drogon)
