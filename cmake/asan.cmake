
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

function(enable_asan_for_target target)
    if(ENABLE_ASAN AND NOT WIN32)
        message(STATUS "Enabling ASAN for target ${target}")
        target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()
endfunction()
