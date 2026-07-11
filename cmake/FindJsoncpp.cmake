# FindJsoncpp.cmake — shim for FetchContent-built jsoncpp
# Placed on CMAKE_MODULE_PATH so Drogon's find_package(Jsoncpp) uses this
# instead of searching system paths.

if(TARGET jsoncpp_static)
    get_target_property(_jsoncpp_inc jsoncpp_static INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _jsoncpp_inc)
        set(_jsoncpp_inc "${jsoncpp_SOURCE_DIR}/include")
    endif()

    set(JSONCPP_FOUND TRUE)
    set(JSONCPP_INCLUDE_DIRS "${_jsoncpp_inc}" CACHE PATH "" FORCE)
    set(JSONCPP_LIBRARIES jsoncpp_static CACHE STRING "" FORCE)

    if(NOT TARGET Jsoncpp_lib)
        add_library(Jsoncpp_lib ALIAS jsoncpp_static)
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Jsoncpp
        REQUIRED_VARS JSONCPP_INCLUDE_DIRS JSONCPP_LIBRARIES
    )
else()
    message(FATAL_ERROR "FindJsoncpp.cmake shim: jsoncpp_static target not found. "
        "Ensure jsoncpp is built via FetchContent before Drogon.")
endif()
