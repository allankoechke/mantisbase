# Use bundled ZLIB package for windows builds
# UNIX builds should pull package maintained version

if(WIN32)
    set(ZLIB_ROOT "${CMAKE_SOURCE_DIR}/3rdParty/zlib")

    target_include_directories(mantisbase PRIVATE
            "${ZLIB_ROOT}/include"
    )

    target_link_directories(mantisbase PRIVATE
            "${ZLIB_ROOT}/lib"
    )

    target_link_libraries(mantisbase PRIVATE
            zlib
    )
endif()