if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)
endif()

include(FetchContent)

# Build jsoncpp from source (required by Drogon)
set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_CMAKE_PACKAGE OFF CACHE BOOL "" FORCE)
set(BUILD_OBJECT_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG 1.9.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(jsoncpp)

# Put our FindJsoncpp.cmake shim on the module path so Drogon uses the
# FetchContent-built jsoncpp instead of searching system paths.
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}")

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

# Use std::filesystem (C++20 is set project-wide)
set(HAS_STD_FILESYSTEM_PATH ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    drogon
    GIT_REPOSITORY https://github.com/drogonframework/drogon.git
    GIT_TAG v1.9.13
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(drogon)

# Drogon's install(EXPORT "DrogonTargets") requires all linked targets to be
# in an export set. Add jsoncpp_static so cmake generate doesn't fail.
install(TARGETS jsoncpp_static EXPORT DrogonTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

target_link_libraries(mantisbase PUBLIC Drogon::Drogon)

if(WIN32)
    target_link_libraries(mantisbase PUBLIC ws2_32 rpcrt4 iphlpapi crypt32)
endif()
