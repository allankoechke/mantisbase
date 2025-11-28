if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(NOT WIN32 AND MANTIS_BUILD_WITH_ASAN) # For now, only enable ASAN for non-windows based builds
        message("Enabling Address Sanitizer")
        include(cmake/asan.cmake)
        set(ENABLE_ASAN ON)

        enable_asan_for_target(mantisbase_lib)
        enable_asan_for_target(mantisbase_app)
    endif()
endif()