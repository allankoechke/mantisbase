set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "")
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "")
set(ZLIB_INSTALL OFF CACHE BOOL "")
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/zlib EXCLUDE_FROM_ALL)

set(ZLIB_ROOT "${CMAKE_SOURCE_DIR}/3rdParty/zlib")
set(ZLIB_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdParty/zlib/include")
set(ZLIB_LIBRARIES "${CMAKE_SOURCE_DIR}/3rdParty/zlib/lib")

target_link_libraries(mantisbase PRIVATE ZLIB::ZLIBSTATIC)
target_include_directories(mantisbase PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/zlib"
        "${CMAKE_CURRENT_BINARY_DIR}/3rdParty/zlib")