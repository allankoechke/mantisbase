# Use bundled ZLIB package for windows builds
# UNIX builds should pull package maintained version
set(ZLIB_USE_STATIC_LIBS ON)
if(WIN32)
    set(ZLIB_ROOT "${CMAKE_SOURCE_DIR}/3rdParty/zlib")
    set(ZLIB_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdParty/zlib/include")
    set(ZLIB_LIBRARIES "${CMAKE_SOURCE_DIR}/3rdParty/zlib/lib")

    target_include_directories(mantisbase PRIVATE
            "${ZLIB_ROOT}/include"
    )

    target_link_directories(mantisbase PRIVATE
            "${ZLIB_ROOT}/lib"
    )
endif()