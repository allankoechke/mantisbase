if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(NOT WIN32 AND MANTIS_BUILD_WITH_ASAN) # For now, only enable ASAN for non-windows based builds
        include(cmake/asan.cmake)
        set(ENABLE_ASAN ON)

        enable_asan_for_target(mantisbase)
        enable_asan_for_target(mantisbase_app)
    endif()
endif()